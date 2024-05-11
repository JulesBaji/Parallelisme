#include <stdio.h>
#define STB_IMAGE_IMPLEMENTATION
#include "lib_stb_image/stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "lib_stb_image/stb_image_write.h"
#include <omp.h>
#include <sys/time.h>

// Fonction pour mesurer le temps écoulé en secondes
double get_time() {
    struct timeval time;
    gettimeofday(&time, NULL);
    return (double)time.tv_sec + (double)time.tv_usec * 1e-6;
}

int main (int argc, char *argv[])
{
    int max_threads = omp_get_max_threads();
    printf("Max threads: %d\n", max_threads);
    // Début de la mesure du temps
    double start_time = get_time();

    if (argc != 3)
    {
        printf("Invalid arguments !\n");
        return EXIT_FAILURE;
    }

    // Get image paths from arguments.
    const char *inputImgPath = argv[1];
    const char *searchImgPath = argv[2];

    // ==================================== Loading input image.
    int inputImgWidth;
    int inputImgHeight;
    int dummyNbChannels; // number of channels forced to 3 in stb_load.
    unsigned char *inputImg = stbi_load(inputImgPath, &inputImgWidth, &inputImgHeight, &dummyNbChannels, 3);
    if (inputImg == NULL)
    {
        printf("Cannot load image %s", inputImgPath);
        return EXIT_FAILURE;
    }
    printf("Input image %s: %dx%d\n", inputImgPath, inputImgWidth, inputImgHeight);

    // ====================================  Loading search image.
    int searchImgWidth;
    int searchImgHeight;
    unsigned char *searchImg = stbi_load(searchImgPath, &searchImgWidth, &searchImgHeight, &dummyNbChannels, 3);
    if (searchImg == NULL)
    {
        printf("Cannot load image %s", searchImgPath);
        return EXIT_FAILURE;
    }
    printf("Search image %s: %dx%d\n", searchImgPath, searchImgWidth, searchImgHeight);

    // ====================================  Convert RGB images to grayscale
    unsigned char *inputGray = (unsigned char *)malloc(inputImgWidth * inputImgHeight);
    unsigned char *searchGray = (unsigned char *)malloc(searchImgWidth * searchImgHeight);
    #pragma omp parallel for collapse(2)
    for (int y = 0; y < inputImgHeight; y++) 
    {
        for (int x = 0; x < inputImgWidth; x++) 
        {
            int index = y * inputImgWidth + x;
            inputGray[index] = 0.299 * inputImg[index * 3] + 0.587 * inputImg[index * 3 + 1] + 0.114 * inputImg[index * 3 + 2];
        }
    }
    #pragma omp parallel for collapse(2)
    for (int y = 0; y < searchImgHeight; y++) 
    {
        for (int x = 0; x < searchImgWidth; x++) 
        {
            int index = y * searchImgWidth + x;
            searchGray[index] = 0.299 * searchImg[index * 3] + 0.587 * searchImg[index * 3 + 1] + 0.114 * searchImg[index * 3 + 2];
        }
    }

    // ====================================  Find the sub-image with minimum SSD
    int minSSD = INT_MAX;
    int minX = 0, minY = 0;
    #pragma omp parallel for collapse(2) shared(minSSD, minX, minY)
    for (int y = 0; y <= inputImgHeight - searchImgHeight; y++) 
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

            #pragma omp critical
            {
                if (sum < minSSD) 
                {
                    minSSD = sum;
                    minX = x;
                    minY = y;
                }
            }
        }
    }

    // ====================================  Draw a red border around the found sub-image.
    #pragma omp parallel for
    for (int j = 0; j < searchImgHeight; j++) 
    {      
        int indexLeft = (minY + j) * inputImgWidth + (minX);
        int indexRight = (minY + j) * inputImgWidth + (minX + searchImgWidth - 1);

        inputImg[indexLeft * 3] = 255; 
        inputImg[indexLeft * 3 + 1] = 0; 
        inputImg[indexLeft * 3 + 2] = 0; 

        inputImg[indexRight * 3] = 255;
        inputImg[indexRight * 3 + 1] = 0;
        inputImg[indexRight * 3 + 2] = 0;
    }

    #pragma omp parallel for
    for (int i = 0; i < searchImgWidth; i++) 
    {
        int indexUp = (minY) * inputImgWidth + (minX + i);
        int indexDown = (minY + searchImgHeight - 1) * inputImgWidth + (minX + i);

        inputImg[indexUp * 3] = 255; 
        inputImg[indexUp * 3 + 1] = 0; 
        inputImg[indexUp * 3 + 2] = 0; 

        inputImg[indexDown * 3] = 255;
        inputImg[indexDown * 3 + 1] = 0;
        inputImg[indexDown * 3 + 2] = 0;
    }

    // Fin de la mesure du temps
    double end_time = get_time();

    // Calcul du temps écoulé
    double elapsed_time = end_time - start_time;

    // Affichage du temps écoulé
    printf("Temps d'exécution : %.6f secondes\n", elapsed_time);

    // ====================================  Save example: save a copy of 'inputImg'
    stbi_write_png("img/save_example.png", inputImgWidth, inputImgHeight, 3, inputImg, inputImgWidth * 3);

    stbi_image_free(inputImg); 
    stbi_image_free(searchImg); 
    free(inputGray);
    free(searchGray);

    printf("Good bye!\n");

    return EXIT_SUCCESS;
}