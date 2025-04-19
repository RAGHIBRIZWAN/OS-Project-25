```c
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>   // For mmap
#include <sys/types.h>
#include <fcntl.h>      // For shm_open
#include <unistd.h>     // For ftruncate
#include <semaphore.h> 

#define SIZE 50
#define SHM_NAME "/chat_shm"
#define SHM_SIZE 1024

typedef struct {
    char uname[30];
    char message[256];
    int chat_active;
    int sender; // 0: user, 1: librarian
} SharedChat;

SharedChat *chat;
sem_t *sem_user_to_lib;
sem_t *sem_lib_to_user;

typedef struct {
    char name[30];
    char author[30];
    int id;
} bookData;

typedef struct {
    char uname[30];
    char name[30];
    char author[30];
    int id;
} issueData;

bookData booksArray[SIZE] = { {"", "", 0} };
issueData issueArray[SIZE] = { {"", "", "", 0} };
int count = 0;
pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER; // Chatgpt

void* authentication(void* arg) {
    int *code = (int *)arg;
    int *result = (int *)malloc(sizeof(int));
    if (*code == 786) {
        *result = 1;
    }
    return (void *)result;
}

void* addBook(void* arg) {
    bookData *data = (bookData *)arg;

    getchar();
    printf("Enter the name of the book: ");
    fgets(data->name, sizeof(data->name), stdin);
    data->name[strcspn(data->name, "\n")] = '\0';  // Remove newline

    printf("Enter the name of the author of the book: ");
    fgets(data->author, sizeof(data->author), stdin);
    data->author[strcspn(data->author, "\n")] = '\0';

    printf("Enter the ID of the book: ");
    scanf("%d", &data->id);
    getchar();

    pthread_mutex_lock(&lock);
    for (int i = 0; i < SIZE; i++) {
        if (booksArray[i].id == 0) break;
        if (strcmp(booksArray[i].name, data->name) == 0 &&
            strcmp(booksArray[i].author, data->author) == 0) {
            printf("Book already exists!\n");
            pthread_mutex_unlock(&lock);
            return NULL;
        }
    }

    for (int i = 0; i < SIZE; i++) {
        if (booksArray[i].id == 0) {
            booksArray[i] = *data;
            printf("Book added!\n");
            break;
        }
    }
    pthread_mutex_unlock(&lock);
    return NULL;
}

void* deleteBook(void* arg) {
    char name[30], author[30];
    getchar();
    printf("Enter the name of the book to delete: ");
    fgets(name, sizeof(name), stdin);
    name[strcspn(name, "\n")] = '\0';

    printf("Enter the author of the book to delete: ");
    fgets(author, sizeof(author), stdin);
    author[strcspn(author, "\n")] = '\0';

    pthread_mutex_lock(&lock);
    for (int i = 0; i < SIZE; i++) {
        if (booksArray[i].id == 0) break;
        if (strcmp(booksArray[i].name, name) == 0 &&
            strcmp(booksArray[i].author, author) == 0) {
            strcpy(booksArray[i].name, "");
            strcpy(booksArray[i].author, "");
            booksArray[i].id = 0;
            printf("Book deleted!\n");
            pthread_mutex_unlock(&lock);
            return NULL;
        }
    }
    printf("Book not found.\n");
    pthread_mutex_unlock(&lock);
    return NULL;
}

void* updateBook(void* arg) {
    bookData *data = (bookData *)arg;
    getchar();
    printf("Enter the name of the book to update: ");
    fgets(data->name, sizeof(data->name), stdin);
    data->name[strcspn(data->name, "\n")] = '\0';

    printf("Enter the author of the book to update: ");
    fgets(data->author, sizeof(data->author), stdin);
    data->author[strcspn(data->author, "\n")] = '\0';

    pthread_mutex_lock(&lock);
    for (int i = 0; i < SIZE; i++) {
        if (booksArray[i].id == 0) break;
        if (strcmp(booksArray[i].name, data->name) == 0 && strcmp(booksArray[i].author, data->author) == 0) {

            printf("Enter new name: ");
            fgets(data->name, sizeof(data->name), stdin);
            data->name[strcspn(data->name, "\n")] = '\0';

            printf("Enter new author: ");
            fgets(data->author, sizeof(data->author), stdin);
            data->author[strcspn(data->author, "\n")] = '\0';

            printf("Enter new ID: ");
            scanf("%d", &data->id);
            getchar();

            booksArray[i] = *data;
            printf("Book updated!\n");
            pthread_mutex_unlock(&lock);
            return NULL;
        }
    }
    printf("Book not found.\n");
    pthread_mutex_unlock(&lock);
    return NULL;
}

void* searchBook(void* arg) {
    char name[30], author[30];
    getchar();
    printf("Enter the name of the book: ");
    fgets(name, sizeof(name), stdin);
    name[strcspn(name, "\n")] = '\0';

    printf("Enter the author of the book: ");
    fgets(author, sizeof(author), stdin);
    author[strcspn(author, "\n")] = '\0';

    pthread_mutex_lock(&lock);
    for (int i = 0; i < SIZE; i++) {
        if (booksArray[i].id == 0) break;
        if (strcmp(booksArray[i].name, name) == 0 &&
            strcmp(booksArray[i].author, author) == 0) {
            printf("Book found!\nName: %s\nAuthor: %s\nID: %d\n",
                   booksArray[i].name, booksArray[i].author, booksArray[i].id);
            pthread_mutex_unlock(&lock);
            return NULL;
        }
    }
    printf("Book not found.\n");
    pthread_mutex_unlock(&lock);
    return NULL;
}

void* issueBook(void* arg) {
    issueData *data = (issueData *)arg;
    getchar();
    
    printf("Enter the name of the user: ");
    fgets(data->uname, sizeof(data->uname), stdin);
    data->uname[strcspn(data->uname, "\n")] = '\0';
    
    printf("Enter the name of the book: ");
    fgets(data->name, sizeof(data->name), stdin);
    data->name[strcspn(data->name, "\n")] = '\0';

    printf("Enter the author of the book: ");
    fgets(data->author, sizeof(data->author), stdin);
    data->author[strcspn(data->author, "\n")] = '\0';

    pthread_mutex_lock(&lock);
    for (int i = 0; i < SIZE; i++) {
        if (booksArray[i].id == 0) break;
        if (strcmp(booksArray[i].name, data->name) == 0 &&
            strcmp(booksArray[i].author, data->author) == 0) {
            issueArray[count] = *data;
            count++;
            printf("Book Issued Successfully!\n");
            pthread_mutex_unlock(&lock);
            return NULL;
        }
    }
    printf("Book not found.\n");
    pthread_mutex_unlock(&lock);
    return NULL;
}

void* returnBook(void* arg) {
    char name[30],author[30],uname[30];
    getchar();
    printf("Enter the name of the user: ");
    fgets(uname, sizeof(uname), stdin);
    uname[strcspn(uname, "\n")] = '\0';
    
    printf("Enter the name of the book: ");
    fgets(name, sizeof(name), stdin);
    name[strcspn(name, "\n")] = '\0';

    printf("Enter the author of the book: ");
    fgets(author, sizeof(author), stdin);
    author[strcspn(author, "\n")] = '\0';

    pthread_mutex_lock(&lock);
    for (int i = 0; i <= count; i++) {
        if (booksArray[i].id == 0) break;
        if (strcmp(issueArray[i].name, name) == 0 && strcmp(issueArray[i].author, author) == 0 && strcmp(issueArray[i].uname, uname) == 0){
            strcpy(issueArray[i].uname, "");
            strcpy(issueArray[i].name, "");
            strcpy(issueArray[i].author, "");
            issueArray[i].id = 0;

            printf("Book Returned Successfully!\n");
            pthread_mutex_unlock(&lock);
            return NULL;
        }
    }
    printf("Book not found.\n");
    pthread_mutex_unlock(&lock);
    return NULL;
}

void* manager(void* arg){
  pthread_mutex_lock(&lock);
  for (int i = 0; i <= count; i++){
    if(issueArray[i].id == 0){
      continue;
    }
    printf("User Name: %s\nBook Name: %s\nAuthor: %s\nID: %d\n", issueArray[i].uname, issueArray[i].name, issueArray[i].author, issueArray[i].id);
  }
  pthread_mutex_unlock(&lock);
  return NULL;
}


void* librarianChat(void* arg) {
    int shm_fd = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0666);
    if (shm_fd == -1) {
        perror("shm_open failed in librarianChat");
        exit(EXIT_FAILURE);
    }

    if (ftruncate(shm_fd, SHM_SIZE) == -1) {
        perror("ftruncate failed in librarianChat");
        exit(EXIT_FAILURE);
    }

    char* buffer = (char *)mmap(0, SHM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (buffer == MAP_FAILED) {
        perror("mmap failed in librarianChat");
        exit(EXIT_FAILURE);
    }

    while (1) {
      sem_wait(sem_user_to_lib);  // Wait for user message

      if (strlen(buffer) > 0) {
          if (strcmp(buffer, "exit") == 0) {
              printf("Chat ended by user.\n");
              break;
          }
          printf("[User]: %s\n", buffer);
      }

      printf("[Librarian]: ");
      fgets(buffer, SHM_SIZE, stdin);
      buffer[strcspn(buffer, "\n")] = '\0';

      sem_post(sem_lib_to_user);  // Notify user of reply

      if (strcmp(buffer, "exit") == 0) {
          break;
      }
    } 

    munmap(buffer, SHM_SIZE);
    close(shm_fd);
    return NULL;
}

void* userChat(void* arg) {
    int shm_fd = shm_open(SHM_NAME, O_RDWR, 0666);
    if (shm_fd == -1) {
        perror("shm_open failed in userChat");
        exit(EXIT_FAILURE);
    }

    if (ftruncate(shm_fd, SHM_SIZE) == -1) {
        perror("ftruncate failed in userChat");
        exit(EXIT_FAILURE);
    }

    char* buffer = (char *)mmap(0, SHM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (buffer == MAP_FAILED) {
        perror("mmap failed in userChat");
        exit(EXIT_FAILURE);
    }

    while (1) {
      printf("[User]: ");
      fgets(buffer, SHM_SIZE, stdin);
      buffer[strcspn(buffer, "\n")] = '\0';

      sem_post(sem_user_to_lib);  // Notify librarian

      if (strcmp(buffer, "exit") == 0) break;

      sem_wait(sem_lib_to_user);  // Wait for librarian reply

      if (strlen(buffer) > 0) {
          printf("[Librarian]: %s\n", buffer);
          buffer[0] = '\0';
      }
    }

    munmap(buffer, SHM_SIZE);
    close(shm_fd);
    return NULL;
}

int main() {
    pthread_t threads[10];
    int code, choice;
    int check = 1;

    sem_user_to_lib = sem_open("/sem_user_to_lib", O_CREAT, 0666, 0);
    sem_lib_to_user = sem_open("/sem_lib_to_user", O_CREAT, 0666, 0);


    if (sem_user_to_lib == SEM_FAILED || sem_lib_to_user == SEM_FAILED) {

        perror("sem_open failed");

        exit(EXIT_FAILURE);

    }

    bookData data;
    issueData issue;
    void* authResult;

    while (check) {
        printf("\nEnter any one:\n 1) Library\n 2) User\n 3) Exit\nChoice: ");
        scanf("%d", &choice);

        switch (choice) {
            case 1:
                printf("Enter the code to access library: ");
                scanf("%d", &code);

                pthread_create(&threads[0], NULL, authentication, &code);
                pthread_join(threads[0], &authResult);

                if (*(int*)authResult == 1) {
                    int choice2;
                    printf("\nEnter any one:\n 1) Add Book\n 2) Delete Book\n 3) Update Book\n 4) Search Book\n 5) Track Users\n 6) Chat\nChoice: ");
                    scanf("%d", &choice2);

                    switch (choice2) {
                        case 1:
                            pthread_create(&threads[1], NULL, addBook, &data);
                            pthread_join(threads[1], NULL);
                            break;
                        case 2:
                            pthread_create(&threads[2], NULL, deleteBook, NULL);
                            pthread_join(threads[2], NULL);
                            break;
                        case 3:
                            pthread_create(&threads[3], NULL, updateBook, &data);
                            pthread_join(threads[3], NULL);
                            break;
                        case 4:
                            pthread_create(&threads[4], NULL, searchBook, NULL);
                            pthread_join(threads[4], NULL);
                            break;
                        case 5:
                            pthread_create(&threads[5], NULL, manager, NULL);
                            pthread_join(threads[5], NULL);
                            break;
                        case 6:
                            pthread_create(&threads[8], NULL, librarianChat, NULL);
                            pthread_join(threads[8], NULL);
                            break;
                        default:
                            printf("Wrong Input!\n");
                            break;
                    }
                } else {
                    printf("Wrong Password!\n");
                }
                break;

            case 2: {
                int choice3;
                printf("\nEnter any one:\n 1) Issue Book\n 2) Return Book\n 3) Chat\nChoice: ");
                scanf("%d", &choice3);

                switch (choice3) {
                    case 1:
                        pthread_create(&threads[6], NULL, issueBook, &issue);
                        pthread_join(threads[6], NULL);
                        break;
                    case 2:
                        pthread_create(&threads[7], NULL, returnBook, NULL);
                        pthread_join(threads[7], NULL);
                        break;
                    case 3:
                        pthread_create(&threads[9], NULL, userChat, NULL);
                        pthread_join(threads[9], NULL);
                        break;
                    default:
                        printf("Wrong Input!\n");
                        break;
                }
                break;
            }

            case 3:
                check = 0;
                break;

            default:
                printf("Wrong Input!\n");
                break;
        }
    }
    free(authResult);

    sem_close(sem_user_to_lib);
    sem_close(sem_lib_to_user);
    sem_unlink("/sem_user_to_lib");
    sem_unlink("/sem_lib_to_user");
    shm_unlink(SHM_NAME);

    return 0;
}
```
