# Recherche de sous-image parallèle (OpenMP, MPI)

Projet de calcul parallèle du master Informatique ISICG (Université de Limoges, 2024), en C.

Le programme recherche la position d'une sous-image dans une image. Il existe en quatre versions pour comparer les approches de parallélisation :

- `sequentielle` : version de référence ;
- `openmp` : parallélisme en mémoire partagée ;
- `mpi` : parallélisme en mémoire distribuée sur un cluster ;
- `hybride` : MPI entre les nœuds, OpenMP dans chaque nœud.

![Temps d'exécution](temps_execution.png)

## Compilation et exécution

```bash
make
./sequentielle
./openmp
./run.sh -n 4 ./mpi        # MPI sur les machines listées dans composition_cluster
./run.sh -n 4 ./hybride
```

Nécessite un compilateur MPI (`mpicc`) avec OpenMP. Les images de test sont dans `img/`. La bibliothèque stb_image est incluse dans `inc/`.

## Technologies

C, OpenMP, MPI.
