//
// Created by CorupTa on 2018-12-25.
//

#include <mpi.h>
#include <stdio.h>

#define PING_PONG_LIMIT 20

int main(int argc, char** argv) {
    MPI_Init(NULL, NULL);

    int world_size;
    MPI_Comm_size(MPI_COMM_WORLD, &world_size);

    int world_rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);

    char processor_name[MPI_MAX_PROCESSOR_NAME];
    int name_len;
    MPI_Get_processor_name(processor_name, &name_len);

    printf("Hello world from processor %s, rank %d out of %d processors\n",
            processor_name, world_rank, world_size);

    /*
     * MPI_Send(
     *  void* data,
     *  int count,
     *  MPI_Datatype datatype,
     *  int destination
     *  int tag,
     *  MPI_Comm communicator
     * )
     *
     * MPI_Recv(
     *  void* data,
     *  int count,
     *  MPI_Datatype datatype,
     *  int source,
     *  int tag,
     *  MPI_Comm communicator,
     *  MPI_Status* status
     * )
     */

    /* BASIC SEND RECEIVE
    int number;
    if (world_rank == 0) {
        number = -1;
        MPI_Send(&number, 1, MPI_INT, 1, 0, MPI_COMM_WORLD);
    } else if (world_rank == 1) {
        MPI_Recv(&number, 1, MPI_INT, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        printf("Process 1 received number %d from process 0\n", number);
    }
    */

    int ping_pong_count = 0;
    int partner_rank = (world_rank + 1) % 2;
    while (ping_pong_count < PING_PONG_LIMIT) {
        if (world_rank == ping_pong_count % 2) {
            ++ping_pong_count;
            MPI_Send(&ping_pong_count, 1, MPI_INT, partner_rank, 0, MPI_COMM_WORLD);
            printf("%d sent and incremented ping_pong_count %d to %d\n",
                    world_rank, ping_pong_count, partner_rank);
        } else {
            MPI_Recv(&ping_pong_count, 1, MPI_INT, partner_rank, 0,
                    MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            printf("%d received ping_pong_count %d from %d\n",
                    world_rank, ping_pong_count, partner_rank);
        }
    }

    MPI_Finalize();
}