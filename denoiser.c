/*
Student Name: Halit Özsoy
Student Number: 2016400141
Compile Status: Compiling
Program Status: Working
Notes: The processes all work as asynchronous as possible. They do not sync after each iteration, instead each
 process returns an immediate result at its current iteration when it's asked for a sum of values from a neighbour.
*/
#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

#define TOTAL_ITERATIONS 5000000
#define MASTER_RANK 0
#define DIRECTIONS 8

/* node and queue is just used to fetch an input text without knowing its dimensions */
/* below is a simple implementation of a standard queue by me */
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
/* queue implementation finishes */

/**
 * generates a random number between 0 and 1.0 (both inclusive)
 * @return (double)0-1.0
 */
double randomProbability() {
    return ((double)rand()) / RAND_MAX;
}

/**
 * Define all MESSAGE_TAGs used in MPI commands instead of using plain integers to increase Readability & Writability
 */
enum MessageType { TOP = 0, RIGHT = 1, BOTTOM = 2, LEFT = 3,
        TOP_RIGHT = 4, BOTTOM_RIGHT = 5, BOTTOM_LEFT = 6, TOP_LEFT = 7,
        ROWS = 20, COLUMNS = 21,
        QUESTION = 500, ANSWER = 600, FINISHED = 700, IMAGE_START = 1000, FINAL_IMAGE_START = 60000 };

/**
 * Simple wrapper for MPI_Send to prevent code repetition
 * @param data
 * @param count
 * @param datatype
 * @param destination
 * @param tag
 */
void sendMessage(void* data, int count, MPI_Datatype datatype, int destination, int tag) {
    MPI_Send(data, count, datatype, destination, tag, MPI_COMM_WORLD);
}

/**
 * Simple wrapper for MPI_Recv to prevent code repetition.
 * @param data
 * @param count
 * @param datatype
 * @param source
 * @param tag
 */
void receiveMessage(void* data, int count, MPI_Datatype datatype, int source, int tag) {
    MPI_Recv(data, count, datatype, source, tag, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
}

/**
 * initializes an answer request for the future for the given neighbour and current process.
 * @param neighbour
 * @param position
 * @param answerRequest
 */
void initializeAnAnswer(int neighbour, int *position, MPI_Request *answerRequest) {
    MPI_Irecv((void*)position, 1, MPI_INT, neighbour, QUESTION, MPI_COMM_WORLD, answerRequest);
}

/**
 * initializes an answer request for the future for all neighbours and current process.
 * an answer request is finished -- received when a neighbour asks the current process a question.
 * that means the current process should reply with an answer response to that neighbour.
 * @param neighbours
 * @param positions
 * @param answerRequests
 * @param answerResponses
 */
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

/**
 * Sum the surroundings of a point in a grid. (Ignores out of boundaries by not considering them in the sum)
 * Also does not include the center point in the sum.
 * @param subImage
 * @param rows
 * @param columns
 * @param rowCenter
 * @param columnCenter
 * @return
 */
int summer(char** subImage, int rows, int columns, int rowCenter, int columnCenter) {
  int sum = 0;
  int i, j;
  for (i = rowCenter - 1; i <= rowCenter + 1; ++i) {
      if (i >= 0 && i < rows) { // if within row boundaries
          for (j = columnCenter - 1; j <= columnCenter + 1; ++j) {
              if (j >= 0 && j < columns) { // if within column boundaries
                  if (i != rowCenter || j != columnCenter) { // skip center
                      sum += (int) subImage[i][j];
                  }
              }
          }
      }
  }
  return sum;
};

/**
 * Test all previously initialized answer requests from neighbours to current process.
 * If any of them is finished (that neighbour asked something for the current process)
 * then, calculate the relevant answer and create an answer response to send it to that neighbour,
 * then, reinitialize that answer request for future questions.
 * @param subImage
 * @param rows
 * @param columns
 * @param neighbours
 * @param positions
 * @param answerRequests
 * @param answerResponses
 */
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
                MPI_Wait(answerResponses+direction, MPI_STATUS_IGNORE);
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
            MPI_Isend((void*)&sum, 1, MPI_INT, neighbours[direction], ANSWER,
                    MPI_COMM_WORLD, answerResponses+direction);
        }
    }

}

/**
 * Notify neighbours that the current process finished all of its iterations and it will terminate
 * when all of its neighbours also finish their iterations.
 * Also receive such information from its neighbours.
 * @param neighbours
 * @param finishedRequests
 * @param finishedResponses
 * @param finishedReqResCount
 */
void sendFinishedAll(int* neighbours, MPI_Request* finishedRequests, MPI_Request* finishedResponses, int *finishedReqResCount) {
    int direction;
    for (direction = 0; direction < DIRECTIONS; ++direction) {
        if (neighbours[direction] != -1) {
            MPI_Isend(NULL, 0, MPI_INT, neighbours[direction], FINISHED, MPI_COMM_WORLD, finishedRequests+(*finishedReqResCount));
            MPI_Irecv(NULL, 0, MPI_INT, neighbours[direction], FINISHED, MPI_COMM_WORLD, finishedResponses+(*finishedReqResCount));
            ++(*finishedReqResCount);
        }
    }
};

/**
 * Check if all of the current process'es neighbours has finished their iterations.
 *
 * Use that function so that
 * "If so, terminate the current process, otherwise keep waiting and answering neighbours"
 * @param neighbours
 * @param finishedRequests
 * @param finishedResponses
 * @param finishedReqResCount
 * @return
 */
int testFinishedAll(int* neighbours, MPI_Request* finishedRequests, MPI_Request* finishedResponses, int *finishedReqResCount) {
    int requestResult = 1, responseResult = 1;
    if (*finishedReqResCount > 0) {
        MPI_Testall(*finishedReqResCount, finishedRequests, &requestResult, MPI_STATUSES_IGNORE);
        MPI_Testall(*finishedReqResCount, finishedResponses, &responseResult, MPI_STATUSES_IGNORE);
    }
    return requestResult && responseResult;
};

/**
 * Ask a question to a neighbour in the given position to calculate the sum for its portion of that position's center.
 * This will create an ask request (MPI_Isend) to notify the neighbour and
 * an ask response (MPI_Irecv) to get the result from the neighbour
 * which will be available when the neighbour answers.
 * @param neighbour
 * @param position
 * @param askRequests
 * @param askReqResCount
 * @param askResponses
 * @param askResponseValues
 */
void askAsync(int neighbour, int position, MPI_Request* askRequests, int *askReqResCount,
        MPI_Request* askResponses, int* askResponseValues
    ) {
    if (neighbour == -1) {
        // no neighbour in this direction
        return;
    }
    MPI_Isend((void*)&position, 1, MPI_INT, neighbour, QUESTION, MPI_COMM_WORLD, askRequests+(*askReqResCount));
    MPI_Irecv((void*)(askResponseValues+(*askReqResCount)), 1, MPI_INT, neighbour, ANSWER,
            MPI_COMM_WORLD, askResponses+(*askReqResCount));
    ++(*askReqResCount);
}

/**
 * Check whether all ask requests & responses are finished by their neighbours so that the calculation can proceed.
 * @param askRequests
 * @param askReqResCount
 * @param askResponses
 * @return
 */
int testAskAll(MPI_Request* askRequests, int *askReqResCount, MPI_Request* askResponses) {
    int requestResult = 1, responseResult = 1;
    if (*askReqResCount > 0) {
        MPI_Testall(*askReqResCount, askRequests, &requestResult, MPI_STATUSES_IGNORE);
        MPI_Testall(*askReqResCount, askResponses, &responseResult, MPI_STATUSES_IGNORE);
    }
    return requestResult && responseResult;
}

/**
 * Get the sum of all ask responses to use in the calculation process,
 * also reduce the number of ask requests/responses back to zero
 * @param askRequests
 * @param askReqResCount
 * @param askResponses
 * @param askResponseValues
 * @return
 */
int askResult(MPI_Request* askRequests, int *askReqResCount, MPI_Request* askResponses, int* askResponseValues) {
    // called after a success testAskAll all ask requests and responses so it's certain that they all did finish.
    int result = 0;
    if((*askReqResCount) > 0) {;
        while (*askReqResCount) {
            --(*askReqResCount);
            result += askResponseValues[(*askReqResCount)];
        }
    }
    return result;
}

/**
 * logic for slave request
 *
 * @param world_size
 * @param world_rank
 * @param beta
 * @param gammaValue
 * @return
 */
int slave(int world_size, int world_rank, double beta, double gammaValue) {
    int iterations = TOTAL_ITERATIONS / (world_size - 1);

    int rows, columns;
    receiveMessage(&rows, 1, MPI_INT, MASTER_RANK, ROWS);
    receiveMessage(&columns, 1, MPI_INT, MASTER_RANK, COLUMNS);

    int neighbours[DIRECTIONS], direction;
    for (direction = 0; direction < DIRECTIONS; ++direction) {
        receiveMessage(neighbours+direction, 1, MPI_INT, MASTER_RANK, direction);
    }

    char* subImage[rows], initialSubImage[rows][columns];
    int i;
    for (i = 0; i < rows; ++i) {
        receiveMessage(initialSubImage[i], columns, MPI_BYTE, MASTER_RANK, IMAGE_START + i);
        subImage[i] = (char*)malloc(columns * sizeof(char));
        memcpy(subImage[i], initialSubImage[i], columns);
    }

    MPI_Request askRequests[DIRECTIONS];
    MPI_Request askResponses[DIRECTIONS];
    MPI_Request answerRequests[DIRECTIONS];
    MPI_Request answerResponses[DIRECTIONS];
    MPI_Request finishedRequests[DIRECTIONS];
    MPI_Request finishedResponses[DIRECTIONS];
    int positions[DIRECTIONS];
    int askResponseValues[DIRECTIONS];

    int askReqResCount = 0, finishedReqResCount = 0;

    /* initialize all answer requests (Irecv for all neighbours for potentials questions in later) */
    initializeAnswers(neighbours, positions, answerRequests, answerResponses);
    /* initialize all answer requests done */
    while(iterations --) {
        if (iterations % 1000000 == 0) {
            printf("slave %d started a new millionth iteration - left: %d\n", world_rank, iterations);
        }
        /* pick a random pixel */
        int rowPosition = rand() % rows;
        int columnPosition = rand() % columns;

        // printf("selected pixel %d / %d  --  %d / %d\n", rowPosition, rows, columnPosition, columns);
        /* pick a random pixel done */
        /* sum neighbour cells */
        int sum = summer(subImage, rows, columns, rowPosition, columnPosition);
        if (rowPosition == 0) {
            askAsync(neighbours[TOP], columnPosition, askRequests, &askReqResCount, askResponses, askResponseValues);
            if (columnPosition == 0) {
                askAsync(neighbours[TOP_LEFT], 0, askRequests, &askReqResCount, askResponses, askResponseValues);
            }
            if (columnPosition == columns - 1) {
                askAsync(neighbours[TOP_RIGHT], 0, askRequests, &askReqResCount, askResponses, askResponseValues);
            }
        }
        if (rowPosition == rows - 1) {
            askAsync(neighbours[BOTTOM], columnPosition, askRequests, &askReqResCount, askResponses, askResponseValues);
            if (columnPosition == 0) {
                askAsync(neighbours[BOTTOM_LEFT], 0, askRequests, &askReqResCount, askResponses, askResponseValues);
            }
            if (columnPosition == columns - 1) {
                askAsync(neighbours[BOTTOM_RIGHT], 0, askRequests, &askReqResCount, askResponses, askResponseValues);
            }
        }
        if (columnPosition == 0) {
            askAsync(neighbours[LEFT], rowPosition, askRequests, &askReqResCount, askResponses, askResponseValues);
        }
        if (columnPosition == columns - 1) {
            askAsync(neighbours[RIGHT], rowPosition, askRequests, &askReqResCount, askResponses, askResponseValues);
        }
        while (!testAskAll(askRequests, &askReqResCount, askResponses)) {
            /* answer neighbours' questions before waiting for answers for its questions -- prevents deadlock */
            answerAll(subImage, rows, columns, neighbours, positions, answerRequests, answerResponses);
            /* answer neighbours' questions done */
        }
        sum += askResult(askRequests, &askReqResCount, askResponses, askResponseValues);
        /* sum neighbour cells done */
        /* calculate delta_e */
        // double deltaE = - 2 * subImage[rowPosition][columnPosition] * (gammaValue * initialSubImage[rowPosition][columnPosition] + beta * sum);
        double deltaE = -2 * gammaValue * initialSubImage[rowPosition][columnPosition] * subImage[rowPosition][columnPosition]
                -2 * beta * subImage[rowPosition][columnPosition] * sum;
        // printf("delta: %f exp delta %f\n", deltaE, exp(deltaE));
        // deltaE = log(accept_probability)  *** accept_probability can be bigger than 1, since we skipped Min(1, acc_prob) ***
        if (log(randomProbability()) <= deltaE) {
            // if accepted, flip the pixel
            subImage[rowPosition][columnPosition] = -subImage[rowPosition][columnPosition];
        }
    }
    // dont finish yet, instead wait until all neighbours also finish
    sendFinishedAll(neighbours, finishedRequests, finishedResponses, &finishedReqResCount);
    while (!testFinishedAll(neighbours, finishedRequests, finishedResponses, &finishedReqResCount)) {
        // some neighbours are not finished yet, keep answering
        answerAll(subImage, rows, columns, neighbours, positions, answerRequests, answerResponses);
    }
    
    for (i = 0; i < rows; ++i) {
        sendMessage(subImage[i], columns, MPI_BYTE, MASTER_RANK, FINAL_IMAGE_START + i);
        free(subImage[i]);
    }
    printf("slave %d finished its work end exited successfully.\n", world_rank);
    return 0;
}

/**
 * logic for master process
 *
 * @param world_size
 * @param world_rank
 * @param input
 * @param output
 * @param grid
 * @return
 */
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
            fprintf(stderr, "Error (Row Mode): rowCount is not divisible by the slave count, "
                            "\"world_size - 1\" = %d where row count is %d\n", world_size - 1, rowCount);
        }
    }
    int slaveRank;
    for (slaveRank = 1; slaveRank <= slaveCount; ++slaveRank) {
        sendMessage(&rowsPerSlave, 1, MPI_INT, slaveRank, ROWS);
        sendMessage(&columnsPerSlave, 1, MPI_INT, slaveRank, COLUMNS);
        int top = slaveRank <= slavesPerRow ? -1 : slaveRank - slavesPerRow;
        int right = slaveRank % slavesPerRow == 0 ? -1 : slaveRank + 1;
        int bottom = slaveRank > slaveCount - slavesPerRow ? -1 : slaveRank + slavesPerRow;
        int left = (slaveRank - 1) % slavesPerRow == 0 ? -1 : slaveRank - 1;
        int topRight = (top == -1 || right == -1) ? -1 : slaveRank - slavesPerRow + 1;
        int bottomRight = (bottom == -1 || right == -1) ? -1 : slaveRank + slavesPerRow + 1;
        int bottomLeft = (bottom == -1 || left == -1) ? -1 : slaveRank + slavesPerRow - 1;
        int topLeft = (top == -1 || left == -1) ? -1 : slaveRank - slavesPerRow - 1;
     //   printf("%d ranks => %d %d %d %d %d %d %d %d\n", slaveRank, top, right, bottom, left, topRight, bottomRight, bottomLeft, topLeft);
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
    printf("All slaves received their input from master, and starting working.\n");


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

/**
 * check arguments and start master/slave function depending on the current process rank.
 * @param argc
 * @param argv
 * @return
 */
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
                            "\"denoiser <input> <output> <beta> <pi> row\n"
            );
            return 1;
        }
        int grid = argc != 6 || strcmp(argv[5], "row") != 0;
        if (grid && sqrt(world_size - 1) * sqrt(world_size - 1) != world_size - 1) {
            fprintf(stderr, "When running in grid mode, the number of slaves "
                            "(number of processors - 1) must be a square number!\n");
            return 1;
        }
        fprintf(stdout, "Running in %s mode.\n", grid ? "grid" : "row");
        if ((error = master(world_size, world_rank, argv[1], argv[2], grid))) {
            fprintf(stderr, "Error in master");
            return error;
        };
    } else { // CALCULATE GAMMA AND RUN SLAVE
        double beta = atof(argv[3]);
        double pi = atof(argv[4]);
        double gammaValue = log((1 - pi) / pi) / 2;
        // named gammaValue instead of gamma bc of the below warning:
        // warning: 'gamma' is deprecated: first deprecated in macOS 10.9 [-Wdeprecated-declarations]
        if ((error = slave(world_size, world_rank, beta, gammaValue))) {
            fprintf(stderr, "Error in slave");
            return error;
        };
    }


    MPI_Finalize();
}
