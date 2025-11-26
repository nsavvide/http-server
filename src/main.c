#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define QUEUE_SIZE 128

typedef struct line_queue {
  char *lines[128];
  int front, rear, count;

  pthread_mutex_t mutex;
  pthread_cond_t cond;
} line_queue_t;

void queue_init(line_queue_t *queue) {
  queue->count = queue->front = queue->rear = 0;
  pthread_mutex_init(&queue->mutex, NULL);
  pthread_cond_init(&queue->cond, NULL);
}

void queue_destroy(line_queue_t *queue) {
  pthread_mutex_destroy(&queue->mutex);
  pthread_cond_destroy(&queue->cond);
}

void queue_push(line_queue_t *queue, char *line) {
  pthread_mutex_lock(&queue->mutex);

  if (queue->count == QUEUE_SIZE) {
    fprintf(stderr, "Queue is full!\n");
    return;
  }

  queue->lines[queue->rear] = line;
  queue->rear = (1 + queue->rear) % QUEUE_SIZE;
  queue->count++;

  pthread_cond_signal(&queue->cond);
  pthread_mutex_unlock(&queue->mutex);
}

char *queue_pop(line_queue_t *queue) {
  pthread_mutex_lock(&queue->mutex);

  while (queue->count == 0) {
    pthread_cond_wait(&queue->cond, &queue->mutex);
  }

  char *line = queue->lines[queue->front];

  queue->front = (queue->front + 1) % QUEUE_SIZE;
  queue->count--;

  pthread_mutex_unlock(&queue->mutex);

  return line;
}

void get_lines(FILE *fin, line_queue_t *queue) {
  char buffer[8];
  char curr_line[1024];
  char curr_len = 0;

  while (fread(buffer, 1, sizeof(buffer), fin) > 0) {
    for (size_t i = 0; i < sizeof(buffer); i++) {
      char c = buffer[i];

      if (c == '\n') {
        curr_line[curr_len] = '\0';
        char *copy_line = strdup(curr_line);
        queue_push(queue, copy_line);

        curr_line[0] = '\0';
        curr_len = 0;
      } else {
        if (curr_len + 1 > sizeof(curr_line)) {
          curr_line[curr_len] = '\0';
          char *copy_line = strdup(curr_line);
          queue_push(queue, copy_line);

          curr_line[0] = '\0';
          curr_len = 0;
        } else {
          curr_line[curr_len] = c;
          curr_len++;
        }
      }
    }
  }

  if (curr_len > 0) {
    curr_line[curr_len] = '\0';
    char *copy_line = strdup(curr_line);
    queue_push(queue, copy_line);
  }

  queue_push(queue, NULL); // signal EOF

  fclose(fin);
}

void *producer_thread(void *arg) {
  struct {
    FILE *fin;
    line_queue_t *queue;
  } *args = arg;

  get_lines(args->fin, args->queue);

  return NULL;
}

int main() {
  FILE *fin = fopen("../messages.txt", "r");

  if (fin == NULL) {
    perror("Error opening file");
    return 1;
  }

  line_queue_t queue;
  queue_init(&queue);

  struct {
    FILE *fin;
    line_queue_t *queue;
  } args = {fin, &queue};

  pthread_t tid;
  pthread_create(&tid, NULL, producer_thread, &args);

  for (;;) {
    char *line = queue_pop(&queue);
    if (line == NULL)
      break;

    printf("%s\n", line);
    free(line);
  }

  pthread_join(tid, NULL);
  queue_destroy(&queue);

  return 0;
}
