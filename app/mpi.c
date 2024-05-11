#include <stdio.h>
#define STB_IMAGE_IMPLEMENTATION
#include "lib_stb_image/stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "lib_stb_image/stb_image_write.h"
#include <omp.h>
#include <sys/time.h>
#include <mpi.h>

typedef struct { 
    int minSSD; 
    int x;
    int y; 
} minSSDCoords;

// Fonction pour la réduction
void minSSDReduction(minSSDCoords *in, minSSDCoords *out, int *len, MPI_Datatype *dptr)
{
    for (int i = 0; i < *len; i++)
    {
        if (in[i].minSSD < out->minSSD)
        {
            out->minSSD = in[i].minSSD;
            out->x = in[i].x;
            out->y = in[i].y;
        }
    }
}

// Fonction pour mesurer le temps écoulé en secondes
double get_time() {
    struct timeval time;
    gettimeofday(&time, NULL);
    return (double)time.tv_sec + (double)time.tv_usec * 1e-6;
}

// Fonction pour convertir une image RGB en niveaux de gris
void greyScale(unsigned char *inputImg, unsigned char *inputGray, int inputImgWidth, int inputImgHeight)
{
    for (int y = 0; y < inputImgHeight; y++) 
    {
        for (int x = 0; x < inputImgWidth; x++) 
        {
            int index = y * inputImgWidth + x;
            inputGray[index] = 0.299 * inputImg[index * 3] + 0.587 * inputImg[index * 3 + 1] + 0.114 * inputImg[index * 3 + 2];
        }
    }
}

// Fonction pour calculer la somme des différences au carré (SSD) entre deux images
void localSSD(unsigned char *inputGray, unsigned char *searchGray, int inputImgWidth, int searchImgWidth, 
    int searchImgHeight, int start, int end, minSSDCoords *in)
{
    for (int y = start; y <= end; y++) 
    {
        for (int x = 0; x <= inputImgWidth - searchImgWidth; x++) 
        {
            int sum = 0;
            for (int j = 0; j < searchImgHeight; j++) 
            {
                for (int i = 0; i < searchImgWidth; i++) 
                {
                    int inputIndex = (y + j) * inputImgWidth + (x + i);
                    int searchIndex = j * searchImgWidth + i;
                    sum += (inputGray[inputIndex] - searchGray[searchIndex])*(inputGray[inputIndex] - searchGray[searchIndex]);
                }
            }

            if (sum < in->minSSD) 
            {
                in->minSSD = sum;
                in->x = x;
                in->y = y;
            }
        }
    }
}

int main (int argc, char *argv[])
{
    // Initialize the MPI environment
    MPI_Init(&argc, &argv);
    // Get the number of processes
    int world_size;
    MPI_Comm_size(MPI_COMM_WORLD, &world_size);
    // Get the rank of the process
    int world_rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);

    unsigned char *inputImg; 
    unsigned char *searchImg; 
    unsigned char *inputGray;
    unsigned char *searchGray;
    int sizes[4];

    // Chargement des images par le processus 0
    if (world_rank == 0)
    {
        // ==================================== Loading input image.
            if (argc != 3)
        {
            printf("Invalid arguments !\n");
            MPI_Finalize();
            return EXIT_FAILURE;
        }

        // Get image paths from arguments.
        const char *inputImgPath = argv[1];
        const char *searchImgPath = argv[2];

        int inputImgWidth;
        int inputImgHeight;
        int dummyNbChannels; // number of channels forced to 3 in stb_load.
        inputImg = stbi_load(inputImgPath, &inputImgWidth, &inputImgHeight, &dummyNbChannels, 3);

        if (inputImg == NULL)
        {
            printf("Cannot load image %s", inputImgPath);
            MPI_Finalize();
            return EXIT_FAILURE;
        }
        printf("Input image %s: %dx%d\n", inputImgPath, inputImgWidth, inputImgHeight);

        // ====================================  Loading search image.
        int searchImgWidth;
        int searchImgHeight;
        searchImg = stbi_load(searchImgPath, &searchImgWidth, &searchImgHeight, &dummyNbChannels, 3);

        if (searchImg == NULL)
        {
            printf("Cannot load image %s", searchImgPath);
            MPI_Finalize();
            return EXIT_FAILURE;
        }
        printf("Search image %s: %dx%d\n", searchImgPath, searchImgWidth, searchImgHeight);

        sizes[0] = inputImgWidth;
        sizes[1] = inputImgHeight;
        sizes[2] = searchImgWidth;
        sizes[3] = searchImgHeight;
    }

    // Début de la mesure du temps
    double start_time = get_time();

    // Broadcast les tailles des images
    MPI_Bcast(sizes, 4, MPI_INT, 0, MPI_COMM_WORLD);
    int inputImgWidth = sizes[0];
    int inputImgHeight = sizes[1];
    int searchImgWidth = sizes[2];
    int searchImgHeight = sizes[3];

    // ====================================  Convert RGB images to grayscale
    inputGray = (unsigned char *)malloc(inputImgWidth * inputImgHeight * sizeof(unsigned char));
    searchGray = (unsigned char *)malloc(searchImgWidth * searchImgHeight * sizeof(unsigned char));
    
    if (world_rank == 0)
    {
        greyScale(inputImg, inputGray, inputImgWidth, inputImgHeight);
        greyScale(searchImg, searchGray, searchImgWidth, searchImgHeight);
        stbi_image_free(searchImg);
    }
    
    // ====================================  Find the sub-image with minimum SSD
    // Broadcast les images en niveaux de gris
    MPI_Bcast(inputGray, inputImgWidth * inputImgHeight, MPI_UNSIGNED_CHAR, 0, MPI_COMM_WORLD);
    MPI_Bcast(searchGray, searchImgWidth * searchImgHeight, MPI_UNSIGNED_CHAR, 0, MPI_COMM_WORLD);

    // Création du type de données pour la structure minSSDCoords
    MPI_Datatype minSSDType;
    MPI_Datatype types[3] = {MPI_INT, MPI_INT, MPI_INT};
    int blocklengths[3] = {1, 1, 1};
    MPI_Aint offsets[3] =
    {
        offsetof(minSSDCoords, minSSD),
        offsetof(minSSDCoords, x),
        offsetof(minSSDCoords, y)
    };

    MPI_Type_create_struct(3, blocklengths, offsets, types, &minSSDType);
    MPI_Type_commit(&minSSDType);

    // Création de l'opération de réduction
    MPI_Op minSSDReductionOp;
    MPI_Op_create((MPI_User_function *)minSSDReduction, 1, &minSSDReductionOp);

    minSSDCoords in, out;

    // Calcul de la taille des chunks
    int hauteurImage = inputImgHeight - searchImgHeight;
    int tailleChunk = hauteurImage / world_size;
    int reste = hauteurImage % tailleChunk;

    hauteurImage = hauteurImage + tailleChunk - reste;
    tailleChunk = hauteurImage / tailleChunk;
    int nbChunks = hauteurImage / tailleChunk;
    
    // Calcul des bornes de la boucle
    int start = world_rank * nbChunks;
    int end = (world_rank == world_size - 1) ? inputImgHeight - searchImgHeight : start + nbChunks;

    // Initialisation de la structure in
    in.minSSD = INT_MAX;
    
    // Calcul de la somme des différences au carré (SSD) entre les images
    localSSD(inputGray, searchGray, inputImgWidth, searchImgWidth, searchImgHeight, start, end, &in);
    
    // Réduction des résultats
    MPI_Reduce( &in, &out, 1, minSSDType, minSSDReductionOp, 0, MPI_COMM_WORLD ); 
    
    // ====================================  Draw a red border around the found sub-image.
    if (world_rank == 0)
    {
        for (int j = 0; j <= searchImgHeight; j++) 
        {      
            int indexLeft = (out.y + j) * inputImgWidth + (out.x);
            int indexRight = (out.y + j) * inputImgWidth + (out.x + searchImgWidth);

            inputImg[indexLeft * 3] = 255; 
            inputImg[indexLeft * 3 + 1] = 0; 
            inputImg[indexLeft * 3 + 2] = 0; 

            inputImg[indexRight * 3] = 255;
            inputImg[indexRight * 3 + 1] = 0;
            inputImg[indexRight * 3 + 2] = 0;
        }

        for (int i = 0; i <= searchImgWidth; i++) 
        {
            int indexUp = (out.y) * inputImgWidth + (out.x + i);
            int indexDown = (out.y + searchImgHeight) * inputImgWidth + (out.x + i);

            inputImg[indexUp * 3] = 255; 
            inputImg[indexUp * 3 + 1] = 0; 
            inputImg[indexUp * 3 + 2] = 0; 

            inputImg[indexDown * 3] = 255;
            inputImg[indexDown * 3 + 1] = 0;
            inputImg[indexDown * 3 + 2] = 0;
        }

        printf("Good bye!\n");

        // Fin de la mesure du temps
        double end_time = get_time();

        // Calcul du temps écoulé
        double elapsed_time = end_time - start_time;

        // Affichage du temps écoulé
        printf("Temps d'exécution : %.6f secondes\n", elapsed_time);
        
        // ====================================  Save example: save a copy of 'inputImg'

        stbi_write_png("img/save_example.png", inputImgWidth, inputImgHeight, 3, inputImg, inputImgWidth * 3);
        stbi_image_free(inputImg); 
    }

    free(inputGray);
    free(searchGray);   

    // Finalize the MPI environment.
    MPI_Type_free(&minSSDType);
    MPI_Op_free(&minSSDReductionOp);
    MPI_Finalize();
    return EXIT_SUCCESS;
}