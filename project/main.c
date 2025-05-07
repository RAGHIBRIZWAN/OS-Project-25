#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>   // For mmap
#include <sys/types.h>
#include <sys/stat.h>
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
    char name[30];
    char password[30];
    char id[10];
} Student;
typedef struct {
    char uname[30];
    char name[30];
    char author[30];
    int id;
} issueData;

char CurrentUser[30];
bookData booksArray[SIZE] = { {"", "", 0} };
issueData issueArray[SIZE] = { {"", "", "", 0} };
int count = 0;
pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

void* authentication(void* arg) {
    int *code = (int *)arg;
    int *result = (int *)malloc(sizeof(int));
    if (*code == 786) {
        *result = 1;
    }
    return (void *)result;
}

// user authentication function 


void* authenticate_user(void* arg) {
    char input_name[30], input_pass[30], input_id[10];
    char file_name_chat[100], file_name_issue[100];
    FILE *file;
    int authenticated = 0;

    while (1) {
        printf("Enter Username: ");
        scanf("%s", input_name);
        printf("Enter Password: ");
        scanf("%s", input_pass);
        printf("Enter ID: ");
        scanf("%s", input_id);

        file = fopen("data.txt", "r");
        if (!file) {
            printf("Error: Could not open data.txt file.\n");
            int* result = malloc(sizeof(int));
            *result = 0;
            return result;
        }

        char line[100];
        Student s;
        while (fgets(line, sizeof(line), file)) {
            // Remove newline if exists
            line[strcspn(line, "\n")] = 0;

            // Split by '|'
            char* token = strtok(line, "|");
            if (token) strncpy(s.name, token, sizeof(s.name));
            token = strtok(NULL, "|");
            if (token) strncpy(s.password, token, sizeof(s.password));
            token = strtok(NULL, "|");
            if (token) strncpy(s.id, token, sizeof(s.id));

            if (strcmp(s.name, input_name) == 0 &&
                strcmp(s.password, input_pass) == 0 &&
                strcmp(s.id, input_id) == 0) {
               strcpy(CurrentUser, input_name);
                
                authenticated = 1;
                break;
            }
        }
        fclose(file);
       


        if (authenticated) {
            printf("Authentication successful!\n");

            struct stat st = {0};
            if (stat("user", &st) == -1) {
                mkdir("user", 0700);
            }

            snprintf(file_name_chat, sizeof(file_name_chat), "user/%sChat.txt", input_name);
            snprintf(file_name_issue, sizeof(file_name_issue), "user/%sIssueBook.txt", input_name);

            if (access(file_name_chat, F_OK) == -1) {
                FILE *chat_file = fopen(file_name_chat, "w");
                if (chat_file) fclose(chat_file);
            }

            if (access(file_name_issue, F_OK) == -1) {
                FILE *issue_file = fopen(file_name_issue, "w");
                if (issue_file) fclose(issue_file);
            }

            break;
        } else {
            
            char choice;
            printf("Authentication failed! Do you want to retry? (y/n): ");
            scanf(" %c", &choice);
            if (choice == 'n' || choice == 'N') {
                printf("Exiting authentication.\n");
                int* result = malloc(sizeof(int));
                *result = 0;
                return result;
            }
        }
    }

    int* result = malloc(sizeof(int));
    *result = 1;
    return result;
}



// for write in fill
void writeBookToFile(bookData *data) {
    FILE *fp = fopen("books.txt", "a");
    if (fp == NULL) {
        perror("Failed to open books.txt");
        return;
    }
    fprintf(fp, "%s|%s|%d\n", data->name, data->author, data->id);
    fclose(fp);
}

void writeIssueToFile(issueData *data) {
    FILE *fp = fopen("issued.txt", "a");
    if (fp == NULL) {
        perror("Failed to open issued.txt");
        return;
    }
    fprintf(fp, "%s|%s|%s|%d\n", data->uname, data->name, data->author, data->id);
    fclose(fp);
}

void rewriteBooksToFile(bookData *books, int count) {
    FILE *fp = fopen("books.txt", "w");
    if (!fp) {
        perror("Error opening books.txt");
        return;
    }
    for (int i = 0; i < count; i++) {
        fprintf(fp, "%s|%s|%d\n", books[i].name, books[i].author, books[i].id);
    }
    fclose(fp);
}




void* addBook(void* arg) {
    bookData *data = (bookData *)arg;

    getchar();
    printf("Enter the name of the book: ");
    fgets(data->name, sizeof(data->name), stdin);
    data->name[strcspn(data->name, "\n")] = '\0';

    printf("Enter the name of the author: ");
    fgets(data->author, sizeof(data->author), stdin);
    data->author[strcspn(data->author, "\n")] = '\0';

    printf("Enter the ID of the book: ");
    scanf("%d", &data->id);
    getchar();

    pthread_mutex_lock(&lock);
    FILE *fp = fopen("books.txt", "r");
    if (fp != NULL) {
        char line[100], name[30], author[30];
        int id;
        while (fgets(line, sizeof(line), fp)) {
            sscanf(line, "%[^|]|%[^|]|%d", name, author, &id);
            if (strcmp(name, data->name) == 0 && strcmp(author, data->author) == 0) {
                printf("Book already exists!\n");
                fclose(fp);
                pthread_mutex_unlock(&lock);
                return NULL;
            }
        }
        fclose(fp);
    }

    writeBookToFile(data);
    printf("Book added!\n");
    pthread_mutex_unlock(&lock);
    return NULL;
}

void* searchBook(void* arg) {
    char name[30];
    int search_id;

    getchar(); // Clear buffer
    printf("Enter the name of the book: ");
    fgets(name, sizeof(name), stdin);
    name[strcspn(name, "\n")] = '\0';

    printf("Enter the ID of the book: ");
    scanf("%d", &search_id);

    pthread_mutex_lock(&lock);
    FILE *fp = fopen("books.txt", "r");
    if (fp == NULL) {
        printf("No books available.\n");
        pthread_mutex_unlock(&lock);
        return NULL;
    }

    char line[100], bname[30], bauthor[30];
    int id;
    int found = 0;

    while (fgets(line, sizeof(line), fp)) {
        sscanf(line, "%[^|]|%[^|]|%d", bname, bauthor, &id);
        if (strcmp(name, bname) == 0 && id == search_id) {
            printf("Book found!\nName: %s\nAuthor: %s\nID: %d\n", bname, bauthor, id);
            found = 1;
            break;
        }
    }

    if (!found) {
        printf("Book not found.\n");
    }

    fclose(fp);
    pthread_mutex_unlock(&lock);
    return NULL;
}


void* deleteBook(void* arg) {
    char name[30];
    int del_id;

    getchar(); // Clear input buffer
    printf("Enter the name of the book to delete: ");
    fgets(name, sizeof(name), stdin);
    name[strcspn(name, "\n")] = '\0';

    printf("Enter the ID of the book to delete: ");
    scanf("%d", &del_id);

    FILE *fp = fopen("books.txt", "r");
    if (!fp) {
        perror("Error opening books.txt");
        return NULL;
    }

    bookData temp[SIZE];
    int count = 0, found = 0;

    while (fscanf(fp, "%[^|]|%[^|]|%d\n", temp[count].name, temp[count].author, &temp[count].id) == 3) {
        if (strcmp(temp[count].name, name) == 0 && temp[count].id == del_id) {
            found = 1;
            continue; // Skip this entry
        }
        count++;
    }
    fclose(fp);

    if (found) {
        rewriteBooksToFile(temp, count);
        printf("Book deleted!\n");
    } else {
        printf("Book not found.\n");
    }

    return NULL;
}


void* updateBook(void* arg) {
    char search_name[30];
    int search_id;

    getchar(); // Clear input buffer
    printf("Enter the name of the book to update: ");
    fgets(search_name, sizeof(search_name), stdin);
    search_name[strcspn(search_name, "\n")] = '\0';

    printf("Enter the ID of the book to update: ");
    scanf("%d", &search_id);
    getchar(); // Clear newline left in buffer

    FILE *fp = fopen("books.txt", "r");
    if (!fp) {
        perror("Error opening books.txt");
        return NULL;
    }

    bookData temp[SIZE];
    int count = 0, found = 0;

    while (fscanf(fp, "%[^|]|%[^|]|%d\n", temp[count].name, temp[count].author, &temp[count].id) == 3) {
        if (strcmp(temp[count].name, search_name) == 0 && temp[count].id == search_id) {
            found = 1;

            printf("Enter new name: ");
            fgets(temp[count].name, sizeof(temp[count].name), stdin);
            temp[count].name[strcspn(temp[count].name, "\n")] = '\0';

            printf("Enter new author: ");
            fgets(temp[count].author, sizeof(temp[count].author), stdin);
            temp[count].author[strcspn(temp[count].author, "\n")] = '\0';

            printf("Enter new ID: ");
            scanf("%d", &temp[count].id);
            getchar(); // clear newline
        }
        count++;
    }
    fclose(fp);

    if (found) {
        rewriteBooksToFile(temp, count);
        printf("Book updated!\n");
    } else {
        printf("Book not found.\n");
    }

    return NULL;
}



void* issueBook(void* arg) {
    issueData *data = (issueData *)arg;
    char input[10];
    int user_found = 0;
    int book_found = 0;

    while (!user_found) {
        getchar();
        printf("Enter the name of the user: ");
        scanf("%s",data->uname);
        //fgets(data->uname, sizeof(data->uname), stdin);
        printf("breakpoint\n");
        printf("%s\n",data->uname);
        data->uname[strcspn(data->uname, "\n")] = '\0';
        FILE *user_fp = fopen("data.txt", "r");
        if (!user_fp) {
            perror("Failed to open data.txt");
            return NULL;
        }

        char u_name[30], u_pass[30], u_id[30];
        while (fscanf(user_fp, "%[^|]|%[^|]|%[^\n]\n", u_name, u_pass, u_id) == 3) {
            if (strcmp(u_name, data->uname) == 0) {
                user_found = 1;
                break;
            }
        }
        fclose(user_fp);

        if (!user_found) {
            printf("User not found! Do you want to try again? (y/n): ");
            fgets(input, sizeof(input), stdin);
            if (input[0] == 'n' || input[0] == 'N') return NULL;
        }
    }

    while (!book_found) {
        printf("Enter the name of the book: ");
        fgets(data->name, sizeof(data->name), stdin);
        data->name[strcspn(data->name, "\n")] = '\0';

        printf("Enter the ID of the book: ");
        scanf("%d", &data->id);
        getchar();  // clear newline

        FILE *book_fp = fopen("books.txt", "r");
        if (!book_fp) {
            perror("Failed to open books.txt");
            return NULL;
        }

        bookData allBooks[SIZE];
        int count = 0;
        bookData temp;
        while (fscanf(book_fp, "%[^|]|%[^|]|%d\n", temp.name, temp.author, &temp.id) == 3) {
            if (strcmp(temp.name, data->name) == 0 && temp.id == data->id) {
                strcpy(data->author, temp.author); // Capture author
                book_found = 1;
                continue; // Skip adding to array so it's deleted
            }
            allBooks[count++] = temp;
        }
        fclose(book_fp);

        if (!book_found) {
            printf("Book not found! Do you want to try again? (y/n): ");
            fgets(input, sizeof(input), stdin);
            if (input[0] == 'n' || input[0] == 'N') return NULL;
        } else {
            // Remove the book from books.txt
            FILE *rewrite_fp = fopen("books.txt", "w");
            for (int i = 0; i < count; i++) {
                fprintf(rewrite_fp, "%s|%s|%d\n", allBooks[i].name, allBooks[i].author, allBooks[i].id);
            }
            fclose(rewrite_fp);
        }
    }

    // Write to global issued.txt
    writeIssueToFile(data);

    // Write to user/<username>IssueBook.txt
    char filepath[100];
    snprintf(filepath, sizeof(filepath), "user/%sIssueBook.txt", data->uname);
    FILE *user_issue_fp = fopen(filepath, "a");
    if (!user_issue_fp) {
        perror("Failed to write to user issue file");
        return NULL;
    }
    fprintf(user_issue_fp, "%s|%s|%s|%d\n", data->uname, data->name, data->author, data->id);
    fclose(user_issue_fp);

    printf("Book Issued Successfully!\n");
    return NULL;
}




void* returnBook(void* arg) {
    char uname[30], name[30], input[10];
    int id;
    int user_found = 0, issued_found = 0;

    // Step 1: Get valid username
    while (!user_found) {
        getchar();
        printf("Enter the name of the user: ");
        fgets(uname, sizeof(uname), stdin);
        uname[strcspn(uname, "\n")] = '\0';

        FILE *user_fp = fopen("data.txt", "r");
        if (!user_fp) {
            perror("Failed to open data.txt");
            return NULL;
        }

        char u_name[30], u_pass[30], u_id[30];
        while (fscanf(user_fp, "%[^|]|%[^|]|%[^\n]\n", u_name, u_pass, u_id) == 3) {
            if (strcmp(u_name, uname) == 0) {
                user_found = 1;
                break;
            }
        }
        fclose(user_fp);

        if (!user_found) {
            printf("User not found! Try again? (y/n): ");
            fgets(input, sizeof(input), stdin);
            if (input[0] == 'n' || input[0] == 'N') return NULL;
        }
    }

    // Step 2: Get book details from user (book name + ID)
    printf("Enter the name of the book: ");
    fgets(name, sizeof(name), stdin);
    name[strcspn(name, "\n")] = '\0';

    printf("Enter the ID of the book: ");
    scanf("%d", &id);
    getchar();

    // Step 3: Search in issued.txt
    FILE *fp = fopen("issued.txt", "r");
    if (!fp) {
        perror("Error opening issued.txt");
        return NULL;
    }

    issueData temp[SIZE];
    int count = 0;
    char found_author[30];  // to store author for later

    while (fscanf(fp, "%[^|]|%[^|]|%[^|]|%d\n", temp[count].uname, temp[count].name, temp[count].author, &temp[count].id) == 4) {
        if (strcmp(temp[count].uname, uname) == 0 &&
            strcmp(temp[count].name, name) == 0 &&
            temp[count].id == id) {
            issued_found = 1;
            strcpy(found_author, temp[count].author); // Save author for books.txt
            continue; // skip this book (removing it)
        }
        count++;
    }
    fclose(fp);

    if (issued_found) {
        // Step 4: Rewrite issued.txt without the returned book
        FILE *out = fopen("issued.txt", "w");
        if (!out) {
            perror("Error writing to issued.txt");
            return NULL;
        }
        for (int i = 0; i < count; i++) {
            fprintf(out, "%s|%s|%s|%d\n", temp[i].uname, temp[i].name, temp[i].author, temp[i].id);
        }
        fclose(out);

        // Step 5: Add book back to books.txt
        FILE *book_out = fopen("books.txt", "a");
        if (!book_out) {
            perror("Error opening books.txt");
            return NULL;
        }
        fprintf(book_out, "%s|%s|%d\n", name, found_author, id);
        fclose(book_out);

        // Step 6: Remove book from user/usernameIssueBook.txt
        char user_file[50];
        snprintf(user_file, sizeof(user_file), "user/%sIssueBook.txt", uname);

        FILE *user_fp = fopen(user_file, "r");
        if (!user_fp) {
            perror("Error opening user's issued file");
            return NULL;
        }

        issueData user_books[SIZE];
        int user_count = 0;
        while (fscanf(user_fp, "%[^|]|%[^|]|%d\n", user_books[user_count].name, user_books[user_count].author, &user_books[user_count].id) == 3) {
            if (strcmp(user_books[user_count].name, name) == 0 && user_books[user_count].id == id) {
                continue; // skip this one
            }
            user_count++;
        }
        fclose(user_fp);

        FILE *user_out = fopen(user_file, "w");
        if (!user_out) {
            perror("Error writing to user's issued file");
            return NULL;
        }
        for (int i = 0; i < user_count; i++) {
            fprintf(user_out, "%s|%s|%d\n", user_books[i].name, user_books[i].author, user_books[i].id);
        }
        fclose(user_out);

        printf("Book returned successfully!\n");
    } else {
        printf("This book is not issued to the user.\n");
    }

    return NULL;
}

void* checkUserIssuedBooks(void* arg) {
   
    char path[100];
    int has_books = 0;

    

    snprintf(path, sizeof(path), "user/%sIssueBook.txt", CurrentUser);

    FILE *fp = fopen(path, "r");
    if (fp == NULL) {
        printf("File is not fine. \n");
        return NULL;
    }

    issueData temp;

    while (fscanf(fp, "%[^|]|%[^|]|%d\n", temp.name, temp.author, &temp.id) == 3) {
        has_books = 1;
        printf("Book Name: %s\nAuthor: %s\nID: %d\n\n", 
               temp.name, temp.author, temp.id);
    }

    fclose(fp);

    if (!has_books) {
        printf("No books are currently issued to you.\n");
    }

    return NULL;
}


void* BookIssuedDetail(void* arg) {
    FILE *fp = fopen("issued.txt", "r");
    if (fp == NULL) {
        perror("Failed to open issued.txt");
        return NULL;
    }

    issueData temp;
    int has_issued_books = 0;

    while (fscanf(fp, "%[^|]|%[^|]|%[^|]|%d\n", temp.uname, temp.name, temp.author, &temp.id) == 4) {
        has_issued_books = 1;
        printf("User Name: %s\nBook Name: %s\nAuthor: %s\nID: %d\n\n", 
               temp.uname, temp.name, temp.author, temp.id);
    }

    fclose(fp);

    if (!has_issued_books) {
        printf("No books are currently issued.\n");
    }

    return NULL;
}


void* Add_User(void* arg) {
    Student newUser;

    printf("Enter user name: ");
    getchar(); // clear buffer
    fgets(newUser.name, sizeof(newUser.name), stdin);
    newUser.name[strcspn(newUser.name, "\n")] = '\0';

    printf("Enter password: ");
    fgets(newUser.password, sizeof(newUser.password), stdin);
    newUser.password[strcspn(newUser.password, "\n")] = '\0';

    printf("Enter user ID: ");
    fgets(newUser.id, sizeof(newUser.id), stdin);
    newUser.id[strcspn(newUser.id, "\n")] = '\0';

    FILE *fp = fopen("data.txt", "a");
    if (fp == NULL) {
        perror("❌ Failed to open data.txt");
        return NULL;
    }

    fprintf(fp, "%s|%s|%s\n", newUser.name, newUser.password, newUser.id);
    fclose(fp);

    printf("✅ User added successfully.\n");

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
    pthread_t threads[12];
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
        printf("\nEnter any one:\n 1) Library\n 2) User\n 3) Exit\n Choice: ");
        scanf("%d", &choice);

        switch (choice) {
            case 1:
                printf("Enter the code to access library: ");
                scanf("%d", &code);

                pthread_create(&threads[0], NULL, authentication, &code);
                pthread_join(threads[0], &authResult);

                if (*(int*)authResult == 1) {
                    int choice2;
                    printf("\nEnter any one:\n 1) Add Book\n 2) Delete Book\n 3) Update Book\n 4) Search Book\n 5) Track Users\n 6) Issue Book\n 7) Return Book\n 8) Add New student\n 9) Chat\nChoice: ");
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
                            pthread_create(&threads[5], NULL, BookIssuedDetail, NULL);
                            pthread_join(threads[5], NULL);
                            break;
                        case 6:
                            pthread_create(&threads[6], NULL, issueBook,NULL);
                            pthread_join(threads[6], NULL);
                            break;
                        case 7:
                            pthread_create(&threads[7], NULL, returnBook, NULL);
                            pthread_join(threads[7], NULL);
                            break;
                        case 8:
                            pthread_create(&threads[8], NULL, Add_User, NULL);
                            pthread_join(threads[8], NULL);
                            break;
                        case 9:
                            pthread_create(&threads[9], NULL, librarianChat, NULL);
                            pthread_join(threads[9], NULL);
                            break;
                        default:
                            printf("Wrong Input!\n");
                            break;
                    }
                } else {
                    printf("Wrong Password!\n");
                }
                break;

            case 2: {// for user side
                int tryAgain = 1;
                while (tryAgain) {
                    pthread_create(&threads[11], NULL, authenticate_user, NULL);
                    pthread_join(threads[11], &authResult);

                    if (*(int*)authResult == 1) {
                        int inMenu = 1;
                        while (inMenu) {
                            int choice3;
                            printf("\nEnter any one:\n 1) Issue Book\n 2) Return Book\n 3) Chat\n 4) Exit\n Choice: ");
                            scanf("%d", &choice3);

                            switch (choice3) {
                                case 1:
                                    pthread_create(&threads[12], NULL, checkUserIssuedBooks, NULL);
                                    pthread_join(threads[12], NULL);
                                    break;
                                case 2:
                                    pthread_create(&threads[7], NULL, returnBook, NULL);
                                    pthread_join(threads[7], NULL);
                                    break;
                                case 3:
                                    pthread_create(&threads[10], NULL, userChat, NULL);
                                    pthread_join(threads[10], NULL);
                                    break;
                                case 4:
                                    inMenu = 0;      // exit from inner loop
                                    tryAgain = 0;    // also exit from outer loop
                                    printf("Exiting...\n");
                                    break;
                                default:
                                    printf("Wrong Input!\n");
                                    break;
                            }
                        }
                    } else {
                        int retryChoice;
                        printf("\nAuthentication failed!\nDo you want to try again?\n 1) Yes\n 2) No (Exit)\n Choice: ");
                        scanf("%d", &retryChoice);
                        if (retryChoice != 1) {
                            tryAgain = 0;
                            printf("Exiting...\n");
                        }
                    }
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
