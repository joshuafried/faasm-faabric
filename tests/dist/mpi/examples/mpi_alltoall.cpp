#include <faabric/mpi/mpi.h>
#include <stdio.h>

namespace tests::mpi {

int allToAll()
{
    MPI_Init(NULL, NULL);

    int rank;
    int worldSize;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &worldSize);

    int chunkSize = 2;
    int fullSize = worldSize * chunkSize;

    // Arrays for sending and receiving
    int* sendBuf = new int[fullSize];
    int* expected = new int[fullSize];
    int* actual = new int[fullSize];

    // Populate data
    for (int i = 0; i < fullSize; i++) {
        // Send buffer from this rank
        sendBuf[i] = (rank * 10) + i;

        // Work out which rank this chunk of the expectation will come from
        int rankOffset = (rank * chunkSize) + (i % chunkSize);
        int recvRank = i / chunkSize;
        expected[i] = (recvRank * 10) + rankOffset;
    }

    MPI_Alltoall(
      sendBuf, chunkSize, MPI_INT, actual, chunkSize, MPI_INT, MPI_COMM_WORLD);

    for (int i = 0; i < fullSize; i++) {
        if (actual[i] != expected[i]) {
            printf("Alltoall failed\n");
            return 1;
        }
    }

    printf("Rank %i: alltoall as expected\n", rank);

    delete[] sendBuf;
    delete[] actual;
    delete[] expected;

    MPI_Finalize();

    return 0;
}
}
