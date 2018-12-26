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
#define DIRECTIONS 8

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

double randomProbability() {
    // generate a number between 0 and 1.0 (both inclusive)
    return ((double)rand()) / RAND_MAX;
}

enum MessageType { TOP = 0, RIGHT = 1, BOTTOM = 2, LEFT = 3,
        TOP_RIGHT = 4, BOTTOM_RIGHT = 5, BOTTOM_LEFT = 6, TOP_LEFT = 7,
        ROWS = 20, COLUMNS = 21,
        QUESTION = 500, ANSWER = 600, IMAGE_START = 1000, FINAL_IMAGE_START = 60000 };

void sendMessage(void* data, int count, MPI_Datatype datatype, int destination, int tag) {
    MPI_Send(data, count, datatype, destination, tag, MPI_COMM_WORLD);
}

void receiveMessage(void* data, int count, MPI_Datatype datatype, int source, int tag) {
    MPI_Recv(data, count, datatype, source, tag, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
}

void initializeAnAnswer(int neighbour, int *position, MPI_Request *answerRequest) {
    MPI_Irecv((void*)position, 1, MPI_INT, neighbour, QUESTION, MPI_COMM_WORLD, MPI_STATUS_IGNORE, answerRequest);
}

void initializeAnswers(int *neighbours, int *positions, MPI_Request* answerRequests, MPI_Request* answerResponses) {
    int direction;
    for (direction = 0; direction < DIRECTIONS; ++direction) {
        if (neighbours[direction] == -1) {
            // no neighbour in this direction
            continue;
        }
        initializeAnAnswer(neighbours[direction], positions+direction, answerRequests+direction);
        answerResponses[direction] = NULL;
    }
}

int summer(char** subImage, int rows, int columns, int rowCenter, int columnCenter) {
  int sum = 0;
  int i, j;
  for (i = rowCenter - 1; i < rowCenter + 1; ++i) {
      if (i >= 0 && i < rows) { // if within row boundaries
          for (j = columnCenter - 1; j < columnCenter + 1; ++j) {
              if (j >= 0 && j < columns) { // if within column boundaries
                  if (i != rowCenter && j != columnCenter) { // skip center
                      sum += (int) subImage[i][j];
                  }
              }
          }
      }
  }
  return sum;
};

void answerAll(char** subImage, int rows, int columns, int *neighbours, int *positions,
        MPI_Request* answerRequests, MPI_Request* answerResponses) {
    int position, direction, rowCenter, columnCenter;
    int flag;
    for (direction = 0; direction < DIRECTIONS; ++direction) {
        if (neighbours[direction] == -1) {
            // no neighbour in this direction
            continue;
        }
        MPI_Test(answerRequests+direction, &flag, MPI_STATUS_IGNORE);
        if (flag) {
            position = positions[direction];
            initializeAnAnswer(neighbours[direction], positions+direction, answerRequests+direction);
            if (answerResponses[direction]) {
                MPI_WAIT(answerResponses[direction], MPI_STATUS_IGNORE);
                answerResponses[direction] = NULL;
            }
            switch (direction) {
                case TOP:
                case TOP_LEFT:
                case TOP_RIGHT:
                    rowCenter = -1;
                    break;
                case BOTTOM:
                case BOTTOM_LEFT:
                case BOTTOM_RIGHT:
                    rowCenter = rows;
                    break;
                case LEFT:
                case RIGHT:
                    rowCenter = position;
                    break;
            }
            switch (direction) {
                case LEFT:
                case TOP_LEFT:
                case BOTTOM_LEFT:
                    columnCenter = -1;
                    break;
                case RIGHT:
                case TOP_RIGHT:
                case BOTTOM_RIGHT:
                    columnCenter = columns;
                    break;
                case TOP:
                case BOTTOM:
                    columnCenter = position;
                    break;
            }
            int sum = summer(subImage, rows, columns, rowCenter, columnCenter);
            MPI_Isend((void)&sum, 1, MPI_INT, neighbours[direction], ANSWER,
                    MPI_COMM_WORLD, answerResponses+direction);
        }
    }

}

void askAsync(int neighbour, int position, MPI_Request* askRequests, int &askRequestCount,
        MPI_Request* askResponses, int &askResponseCount, int* askResponseValues
    ) {
    if (neighbour == -1) {
        // no neighbour in this direction
        return;
    }
    MPI_Isend((void*)&position, 1, MPI_INT, neighbour, QUESTION, MPI_COMM_WORLD, askRequests+askRequestCount);
    ++askRequestCount;
    MPI_Irecv((void*)(askResponseValues+askResponseCount), 1, MPI_INT, neighbour, ANSWER,
            MPI_COMM_WORLD, MPI_STATUS_IGNORE, askResponses+askResponseCount);
    ++askResponseCount;
}

int askWaitResult(MPI_Request* askResponses, int &askResponseCount, int* askResponseValues) {
    int result = 0;
    if(askResponseCount > 0) {
        MPI_Waitall(askResponseCount, askResponses, MPI_STATUSES_IGNORE);
        while (askResponseCount --) {
            result += askResponseValues[askResponseCount];
        }
    }
    return result;
}

int slave(int world_size, int world_rank, double beta, double pi) {
    int iterations = TOTAL_ITERATIONS / world_size;

    int rows, columns;
    receiveMessage(&rows, 1, MPI_INT, MASTER_RANK, ROWS);
    receiveMessage(&columns, 1, MPI_INT, MASTER_RANK, COLUMNS);

    int neighbours[DIRECTIONS], direction;
    for (direction = 0; direction < DIRECTIONS; ++i) {
        receiveMessage(neighbours+direction, 1, MPI_INT, MASTER_RANK, direction);
    }

    char subImage[rows][columns], initialSubImage[row][columns], i;
    for (i = 0; i < rows; ++i) {
        receiveMessage(subImage[i], columns, MPI_BYTE, MASTER_RANK, IMAGE_START + i);
        memcpy(initialSubImage[i], subImage[i], columns);
    }

    MPI_Request askRequests[DIRECTIONS];
    MPI_Request askResponses[DIRECTIONS];
    MPI_Request answerRequests[DIRECTIONS];
    MPI_Request answerResponses[DIRECTIONS];
    int positions[DIRECTIONS];
    int askResponseValues[DIRECTIONS];

    int askRequestCount = 0, askResponseCount = 0;

    /* initialize all answer requests (Irecv for all neighbours for potentials questions in later) */
    initializeAnswers(neighbours, positions, answerRequests, answerResponses);
    /* initialize all answer requests done */
    while(iterations --) {
        /* pick a random pixel */
        int rowPosition = rand() % rows;
        int columnPosition = rand() % columns;
        /* pick a random pixel done */
        /* sum neighbour cells */
        int sum = summer(subImage, rows, columns, rowPosition, columnPosition);
        if (rowPosition == 0) {
            askAsync(neighbours[TOP], columnPosition, askRequests, askRequestCount, askResponses, askResponseCount, askResponseValues);
            if (columnPosition == 0) {
                askAsync(neighbours[TOP_LEFT], 0, askRequests, askRequestCount, askResponses, askResponseCount, askResponseValues);
            }
            if (columnPosition == columns - 1) {
                askAsync(neighbours[TOP_RIGHT], 0, askRequests, askRequestCount, askResponses, askResponseCount, askResponseValues);
            }
        }
        if (rowPosition == rows - 1) {
            askAsync(neighbours[BOTTOM], columnPosition, askRequests, askRequestCount, askResponses, askResponseCount, askResponseValues);
            if (columnPosition == 0) {
                askAsync(neighbours[BOTTOM_LEFT], 0, askRequests, askRequestCount, askResponses, askResponseCount, askResponseValues);
            }
            if (columnPosition == columns - 1) {
                askAsync(neighbours[BOTTOM_RIGHT], 0, askRequests, askRequestCount, askResponses, askResponseCount, askResponseValues);
            }
        }
        if (columnPosition == 0) {
            askAsync(neighbours[LEFT], rowPosition, askRequests, askRequestCount, askResponses, askResponseCount, askResponseValues);
        }
        if (columnPosition == columns - 1) {
            askAsync(neighbours[RIGHT], rowPosition, askRequests, askRequestCount, askResponses, askResponseCount, askResponseValues)
        }
        /* answer neighbours' questions before waiting for answers for its questions -- prevents deadlock */
        answerAll(subImage, rows, columns, neighbours, positions, answerRequests, answerResponses);
        /* answer neighbours' questions done */
        sum += askWaitResult(askResponses, askResponseCount, askResponseValues);
        /* sum neighbour cells done */
        /* calculate delta_e */
        double deltaE = - 2 * subImage[i][j] * (gamma * initialSubImage[i][j] + beta * sum);
        // delteE = log(accept_probability)  *** accept_probability can be bigger than 1, since we skipped Min(1, acc_prob) ***
        if (log(randomProbability()) <= deltaE) {
            // if accepted, flip the pixel
            subImage[i][j] = -subImage[i][j];
        }
    }

    for (i = 0; i < rows; ++i) {
        sendMessage(subImage[i], columns, MPI_BYTE, MASTER_RANK, FINAL_IMAGE_START + i);
    }
    return 0;
}

int master(int world_size, int world_rank, char* input, char* output, int grid) {
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
        int topRight = (top == -1 || right == -1) ? -1 : slaveRank - slavesPerRow + 1;
        int bottomRight = (bottom == -1 || right == -1) ? -1 : slaveRank + slavesPerRow + 1;
        int bottomLeft = (bottom == -1 || left == -1) ? -1 : slaveRank + slavesPerRow - 1;
        int topLeft = (top == -1 || left == -1) ? -1 : slaveRank - slavesPerRow - 1;
        sendMessage(&top, 1, MPI_INT, slaveRank, TOP);
        sendMessage(&right, 1, MPI_INT, slaveRank, RIGHT);
        sendMessage(&bottom, 1, MPI_INT, slaveRank, BOTTOM);
        sendMessage(&left, 1, MPI_INT, slaveRank, LEFT);
        sendMessage(&topRight, 1, MPI_INT, slaveRank, TOP_RIGHT);
        sendMessage(&bottomRight, 1, MPI_INT, slaveRank, BOTTOM_RIGHT);
        sendMessage(&bottomLeft, 1, MPI_INT, slaveRank, BOTTOM_LEFT);
        sendMessage(&topLeft, 1, MPI_INT, slaveRank, TOP_LEFT);
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
    // MPI INITIALIZATIONS
    MPI_Init(NULL, NULL);
    int world_size;
    MPI_Comm_size(MPI_COMM_WORLD, &world_size);
    int world_rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);

    int error = 0;
    srand(time(NULL));

    if (world_rank == MASTER_RANK) { // VALIDATIONS & RUN MASTER
        /* make arg checks in master to prevent duplicate error logs */
        if (argc < 5 || argc > 6) {
            fprintf(stderr, "Please, run the program as \n"
                            "\"denoiser <input> <output> <beta> <pi>\", or as \n"
                            "\"denoiser <input> <output> <beta> <pi> grid\n"
            );
            return 1;
        }
        int grid = argc == 6 && strcmp(argv[5], "grid") == 0;
        if (grid && sqrt(world_size - 1) * sqrt(world_size - 1) != world_size - 1) {
            fprintf(stderr, "When running in grid mode, the number of slaves "
                            "(number of processors - 1) must be a square number!\n");
            return 1;
        }
        fprintf(stdout, "Running in %s mode.", grid ? "grid" : "row");
        if ((error = master(world_size, world_rank, argv[1], argv[2], grid))) {
            fprintf(stderr, "Error in master");
            return error;
        };
    } else { // CALCULATE GAMMA AND RUN SLAVE
        double beta = atof(argv[3]);
        double pi = atof(argv[4]);
        double gamma = log((1 - pi) / pi) / 2;
        if ((error = slave(world_size, world_rank, beta, gamma))) {
            fprintf(stderr, "Error in slave");
            return error;
        };
    }


    MPI_Finalize();
}
