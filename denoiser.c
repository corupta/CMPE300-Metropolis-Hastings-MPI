/*
Student Name: Halit Özsoy
Student Number: 2016400141
Compile Status: Compiling
Program Status: TODO - NOT FINISHED
Notes: ...
*/
#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define TOTAL_ITERATIONS 5000000
#define MASTER_RANK 0

typedef struct node {
    void *val;
    struct node* next;
} node;
typedef struct queue {
    node* head;
    node* tail;
} queue;
node* newNode() {
    node *n = (node*)malloc(sizeof(node));
    n->val = NULL;
    n->next = n;
    return n;
}
queue* newQueue() {
    queue *q = (queue*)malloc(sizeof(queue));
    q->head = newNode();
    q->tail = q->head;
    return q;
}
void push(queue* q, char* val) {
    q->tail->next = newNode();
    q->tail->val = val;
    q->tail = q->tail->next;
}
void* pop(queue *q) {
    void* res = q->head->val;
    node* old = q->head;
    q->head = q->head->next;
    if (q->head != old) {
        free(old);
    }
    return res;
}
void freeQueue(queue *q) {
    void *val;
    while((val = pop(q))) {
        free(val);
    };
    free(q->head);
    free(q);
}

enum MessageType { ROWS = 0, COLUMNS = 1, TOP = 7, RIGHT = 8, BOTTOM = 9, LEFT = 10,
        IMAGE_START = 1000, FINAL_IMAGE_START = 60000 };

void sendMessage(void* data, int count, MPI_Datatype datatype, int destination, int tag) {
    MPI_Send(data, count, datatype, destination, tag, MPI_COMM_WORLD);
}

void receiveMessage(void* data, int count, MPI_Datatype datatype, int source, int tag) {
    MPI_Recv(data, count, datatype, source, tag, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
}

int slave(int world_size, int world_rank, double beta, double pi) {
    int iterations = TOTAL_ITERATIONS / world_size;

    int rows, columns;
    receiveMessage(&rows, 1, MPI_INT, MASTER_RANK, ROWS);
    receiveMessage(&columns, 1, MPI_INT, MASTER_RANK, COLUMNS);

    int topNeighbour, leftNeighbour, bottomNeighbour, rightNeighbour;
    receiveMessage(&topNeighbour, 1, MPI_INT, MASTER_RANK, TOP);
    receiveMessage(&rightNeighbour, 1, MPI_INT, MASTER_RANK, RIGHT);
    receiveMessage(&bottomNeighbour, 1, MPI_INT, MASTER_RANK, BOTTOM);
    receiveMessage(&leftNeighbour, 1, MPI_INT, MASTER_RANK, LEFT);

    int subImage[rows][columns], i;
    for (i = 0; i < rows; ++i) {
        receiveMessage(subImage[i], columns, MPI_BYTE, MASTER_RANK, IMAGE_START + i);
    }

    while(iterations --) {
        int i = rand() % rows;
        int j = rand() % columns;
        subImage[i][j] = -subImage[i][j];
    }

    /* do stuff */
    printf("slave %d says hi!\n", world_rank);


    for (i = 0; i < rows; ++i) {
        sendMessage(subImage[i], columns, MPI_BYTE, MASTER_RANK, FINAL_IMAGE_START + i);
    }
    return 0;
}

int master(int world_size, int world_rank, char* input, char* output, double beta, double pi, int grid) {
    FILE *inputFile, *outputFile;
    inputFile = fopen(input, "r");

    queue* rowQueue = newQueue();
    int rowCount = 0;
    int columnCount = 0;

    char *line = NULL;
    size_t len = 0;
    ssize_t read;

    while ((read = getline(&line, &len, inputFile)) != -1) {
        int i, cursor = 0, nextCursor, nextPixel;
        char* row = NULL;
        if (columnCount == 0) {
            queue* columnQueue = newQueue();
            while (sscanf(line + cursor, "%d%n", &nextPixel, &nextCursor) > 0) {
                cursor += nextCursor;
                ++columnCount;
                push(columnQueue, (void*)nextPixel);
            }
            row = (char*)malloc(columnCount * sizeof(char));
            for (i = 0; i < columnCount; ++i) {
                row[i] = (char)pop(columnQueue);
            }
            freeQueue(columnQueue);
        } else {
            row = (char*)malloc(columnCount * sizeof(char));
            i = 0;
            while(sscanf(line + cursor, "%d%n", &nextPixel, &nextCursor) > 0) {
                cursor += nextCursor;
                row[i++] = (char)nextPixel;
            }
        }
        ++rowCount;
        push(rowQueue, (void*)row);
    }

    int slaveCount = world_size - 1;
    int rowsPerSlave, columnsPerSlave, slavesPerRow;
    if (grid) {
        int sqrtSlaveCount = sqrt(slaveCount);
        rowsPerSlave = rowCount / sqrtSlaveCount;
        columnsPerSlave = columnCount / sqrtSlaveCount;
        slavesPerRow = sqrtSlaveCount;
        if (rowsPerSlave * sqrtSlaveCount != rowCount
            || columnsPerSlave * sqrtSlaveCount != columnCount) {
            fprintf(stderr, "Error (Grid Mode): rowCount or columnCount is not divisible "
                            "by the square root of slave count, \"sqrt(world_size - 1)\"\n");
            return 1;
        }
    } else {
        rowsPerSlave = rowCount / slaveCount;
        columnsPerSlave = columnCount;
        slavesPerRow = 1;
        if (rowsPerSlave * slaveCount != rowCount) {
            fprintf(stderr, "Error (Row Mode): rowCount is not divisible by the slave count, \"world_size - 1\"\n");
        }
    }
    int slaveRank;
    for (slaveRank = 1; slaveRank <= slaveCount; ++slaveRank) {
        sendMessage(&rowsPerSlave, 1, MPI_INT, slaveRank, ROWS);
        sendMessage(&columnsPerSlave, 1, MPI_INT, slaveRank, COLUMNS);
        int top = slaveRank <= slavesPerRow ? -1 : slaveRank - slavesPerRow;
        int right = slaveRank % slavesPerRow == 0 ? -1 : slaveRank + 1;
        int bottom = slaveRank > slaveCount - slavesPerRow ? -1 : slaveRank + slavesPerRow;
        int left = slaveRank % slavesPerRow == 1 ? -1 : slaveRank + 1;
        sendMessage(&top, 1, MPI_INT, slaveRank, TOP);
        sendMessage(&right, 1, MPI_INT, slaveRank, RIGHT);
        sendMessage(&bottom, 1, MPI_INT, slaveRank, BOTTOM);
        sendMessage(&left, 1, MPI_INT, slaveRank, LEFT);
    }
    char* row;
    int rowNumber = 0, slaveRowNumber, columnNumber;
    while((row = (char*)pop(rowQueue))) {
        int slaveRankStart = (rowNumber / rowsPerSlave) * slavesPerRow + 1;
        int slaveRowNumber = rowNumber % rowsPerSlave;
        for (columnNumber = 0; columnNumber < columnCount; columnNumber += columnsPerSlave) {
            slaveRank = slaveRankStart + columnNumber / columnsPerSlave;
            sendMessage(row + columnNumber, columnsPerSlave, MPI_BYTE, slaveRank, IMAGE_START + slaveRowNumber);
        }
        free(row);
        ++rowNumber;
    }
    freeQueue(rowQueue);
    printf("let the slaves work now\n");


    char finalResult[rowCount][columnCount];
    for (rowNumber = 0; rowNumber < rowCount; ++rowNumber) {
        for (columnNumber = 0; columnNumber < columnCount; columnNumber += columnsPerSlave) {
            slaveRank = (rowNumber / rowsPerSlave) * slavesPerRow + columnNumber / columnsPerSlave + 1;
            slaveRowNumber = rowNumber % rowsPerSlave;
            receiveMessage(finalResult[rowNumber] + columnNumber, columnsPerSlave,
                    MPI_BYTE, slaveRank, FINAL_IMAGE_START + slaveRowNumber);
        }
    }

    printf("finished calculations and communciations, started writing to output\n");
    outputFile = fopen(output, "w");
    for (rowNumber = 0; rowNumber < rowCount; ++rowNumber) {
        for (columnNumber = 0; columnNumber < columnCount; ++columnNumber) {
            fprintf(outputFile, "%d ", (int)finalResult[rowNumber][columnNumber]);
        }
        fprintf(outputFile, "\n");
    }
    printf("finished successfully!\n");
    return 0;
}

int main(int argc, char** argv) {
    if (argc < 5 || argc > 6) {
        fprintf(stderr, "Please, run the program as \n"
                        "\"denoiser <input> <output> <beta> <pi>\", or as \n"
                        "\"denoiser <input> <output> <beta> <pi> grid\n"
        );
        return 1;
    }
    int grid = argc == 6 && !strcmp(argv[5], "grid");

    MPI_Init(NULL, NULL);

    int world_size;
    MPI_Comm_size(MPI_COMM_WORLD, &world_size);

    if (grid && sqrt(world_size - 1) * sqrt(world_size - 1) != world_size - 1) {
        fprintf(stderr, "When running in grid mode, the number of slaves "
                        "(number of processors - 1) must be a square number!\n");
        return 1;
    }

    int world_rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);

    /*
    char processor_name[MPI_MAX_PROCESSOR_NAME];
    int name_len;
    MPI_Get_processor_name(processor_name, &name_len);
    */
    int error = 0;
    if (world_rank == MASTER_RANK) {
        if ((error = master(world_size, world_rank, argv[1], argv[2],
                atof(argv[3]), atof(argv[4]), grid))) {
            fprintf(stderr, "Error in master");
            return error;
        };
    } else {
        if ((error = slave(world_size, world_rank, atof(argv[3]), atof(argv[4])))) {
            fprintf(stderr, "Error in slave");
            return error;
        };
    }


    MPI_Finalize();
}
