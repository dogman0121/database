#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <windows.h>


#define COLUMN_NAME_LENGTH 16
#define COLUMN_SURNAME_LENGTH 16
#define COLUMN_PATRONYMIC_LENGTH 16
#define COLUMN_DATE_LENGTH 16
#define COLUMN_EXAMS_LENGTH 5
#define COLUMN_TEST_LENGTH 80
#define COLUMN_EXAM_LENGTH 84
#define COLUMN_TESTS_LENGTH 10
#define COLUMN_SUBJECT_NAME_LENGTH 64


typedef enum {
    EXECUTION_SUCCEED,
    EXECUTION_FAILED
} ExecutionResult;

typedef enum {
    PREPARE_SYNTAX_ERROR,
    PREPARE_SUCCEED,
    PREPARE_UNRECOGNIZED_COMMAND
} PrepareStatement;


typedef enum {
    STATEMENT_APPEND,
    STATEMENT_SET,
    STATEMENT_CANCEL,
    STATEMENT_CONFIRM,
    STATEMENT_SELECT,
    STATEMENT_DELETE,
    STATEMENT_GET,
    STATEMENT_ADD,
    STATEMENT_REMOVE,
    STATEMENT_SORT
} StatementType;

typedef struct {
    StatementType type;
    char *field;
    void *value;
    void *row;
} Statement;

typedef struct {
    char* buffer;
    int input_length;
    int buffer_length;
} InputBuffer;

typedef struct {
    int file_descriptor;
    int nums_rows;
    void **rows;
} DataBase;

typedef struct {
    char name[COLUMN_SUBJECT_NAME_LENGTH];
    char date[COLUMN_DATE_LENGTH];
} Test;

typedef struct {
    char name[COLUMN_SUBJECT_NAME_LENGTH];
    char date[COLUMN_DATE_LENGTH];
    int mark;
} Exam;

typedef struct {
    char name[COLUMN_NAME_LENGTH];
    char surname[COLUMN_SURNAME_LENGTH];
    char patronymic[COLUMN_PATRONYMIC_LENGTH];
    char date_of_birthday[COLUMN_DATE_LENGTH];
    Exam exams[COLUMN_EXAMS_LENGTH];
    Test tests[COLUMN_TESTS_LENGTH];
    int tests_length;
    int exams_length;
} Student;

typedef struct {
    void *row;
    int num_row;
    bool new_row;
} Cursor;

bool CREATING_STUDENT = false;
bool EDITING_STUDENT = false;

InputBuffer* new_input_buffer() {
    InputBuffer* input_buffer = malloc(sizeof(InputBuffer));
    input_buffer->buffer = NULL;
    input_buffer->buffer_length = 0;
    input_buffer->input_length = 0;

    return input_buffer;
}

ssize_t getline(char **lineptr, size_t *n, FILE *stream) {
    size_t pos = 0;
    int c;

    if (!lineptr || !n || !stream) {
        return -1;
    }

    if (*lineptr == NULL) {
        *n = 128;
        *lineptr = (char*)malloc(*n);
        if (*lineptr == NULL) {
            return -1;
        }
    }

    while ((c = fgetc(stream)) != EOF) {
        (*lineptr)[pos++] = c;
        if (pos >= *n) {
            *n *= 2;
            *lineptr = (char*)realloc(*lineptr, *n);
            if (*lineptr == NULL) {
                return -1;
            }
        }
        if (c == '\n') {
            break;
        }
    }

    (*lineptr)[pos] = '\0';

    if (pos == 0) {  // если строка пуста
        return -1;
    }

    return pos;
}

void read_input(InputBuffer* input_buffer) {
    ssize_t bytes_read =
            getline(&(input_buffer->buffer), &(input_buffer->buffer_length), stdin);

    if (bytes_read <= 0) {
        printf("Error reading input\n");
        exit(EXIT_FAILURE);
    }

    // Ignore trailing newline
    input_buffer->input_length = bytes_read - 1;
    input_buffer->buffer[bytes_read - 1] = 0;
}

void print_prompt() { printf("db >"); }

void* get_row(DataBase *db, int num){
    Student* student = malloc(sizeof(Student));
    lseek(db -> file_descriptor, num*sizeof(Student), SEEK_SET);
    read(db -> file_descriptor, student, sizeof(Student));
    return student;
}

DataBase* db_open(char *filename){
    int fd = open(filename, O_RDWR | O_CREAT);
    if (fd <= -1){
        return NULL;
    }

    DataBase *db = (DataBase*)malloc(sizeof(DataBase));

    db -> file_descriptor = fd;
    off_t file_length = lseek(db -> file_descriptor, 0, SEEK_END);

    db -> nums_rows = file_length / sizeof (Student);
    db -> rows = malloc(sizeof(Student*) * (db -> nums_rows));

    for (int i = 0; i < db -> nums_rows; i++){
        db -> rows[i] = get_row(db, i);
    }
    return db;
}


void db_close(DataBase *db){
    for (int i = 0; i > db -> nums_rows; i++){
        free (db -> rows[i]);
    }
    free(db);
    close(db -> file_descriptor);
}

void db_save(DataBase *db){
    for (int i = 0; i < db -> nums_rows; i++){
        lseek(db -> file_descriptor, sizeof(Student)*i, SEEK_SET);
        Student *s = db -> rows[i];
        write(db -> file_descriptor, db -> rows[i], sizeof(Student));
    }
}

void add_row(DataBase *db,  Student *student) {
    db -> rows = realloc(db -> rows, (db -> nums_rows + 1) * sizeof(Student));
    db -> rows [db -> nums_rows] = student;
    db -> nums_rows += 1;
}

void delete_row(DataBase *db, int num_row) {
    for (int i = num_row-1; i < db -> nums_rows-1; i++)
        db -> rows[i] = db -> rows[i+1];

    free(db -> rows[db -> nums_rows -1]);
    db -> nums_rows -= 1;
    db -> rows = realloc(db -> rows, sizeof(Student) * db -> nums_rows);
}

void print_row(Student *student) {
    printf("(%s, %s, %s)\n",
           student -> name, student -> surname, student -> patronymic);
}


void print_student_information(Student *student) {
    printf("Name: %s\n", student -> name);
    printf("Surname: %s\n", student -> surname);
    printf("Patronymic: %s\n", student -> patronymic);
    printf("Date of Birthday: %s\n", student -> date_of_birthday);
    printf("Exams:\n");
    for (int i = 0; i < student -> exams_length; i++) {
        printf("(%s, %s, %d)\n", student -> exams[i].name,student -> exams[i].date, student -> exams[i].mark);
    }
    printf("Tests:\n");
    for (int i = 0; i < student -> tests_length; i++) {
        printf("(%s, %s)\n", student -> tests[i].name,student -> tests[i].date);
    }
}

Student* new_student(){
    Student *student = (Student*) malloc(sizeof(Student));
    student -> exams_length = 0;
    student -> tests_length = 0;
    memset(student -> name, 0, COLUMN_NAME_LENGTH);
    memset(student -> surname, 0, COLUMN_SURNAME_LENGTH);
    memset(student -> patronymic, 0, COLUMN_PATRONYMIC_LENGTH);
    memset(student -> date_of_birthday, 0, COLUMN_DATE_LENGTH);
    memset(student -> exams, 0, COLUMN_EXAMS_LENGTH * COLUMN_EXAM_LENGTH);
    memset(student -> tests, 0, COLUMN_TESTS_LENGTH * COLUMN_TEST_LENGTH);
    return student;
}

Test* new_test() {
    Test *t = (Test*)calloc(1, sizeof(Test));
    return t;
}

Exam* new_exam() {
    Exam *t = (Exam*)calloc(1, sizeof(Exam));
    return t;
}

ExecutionResult execute_set(Student *student, char *field, char *value){
    if (strcmp(field, "name") == 0)
        strcpy(student->name, value);
    if (strcmp(field, "surname") == 0)
        strcpy(student->surname, value);
    if (strcmp(field, "patronymic") == 0)
        strcpy(student->patronymic, value);
    if (strcmp(field, "date_of_birthday") == 0)
        strcpy(student->date_of_birthday, value);

    print_student_information(student);
    return EXECUTION_SUCCEED;
}

ExecutionResult execute_confirm(DataBase *db, Cursor *cursor){
    if (CREATING_STUDENT){
        add_row(db, cursor -> row);
        db_save(db);
        cursor -> num_row = false;
        CREATING_STUDENT = false;
    }
    else if (EDITING_STUDENT){
        EDITING_STUDENT = false;
    }
}

ExecutionResult execute_append(Cursor *cursor){
    printf("You have entered the user creating mode.\n");
    CREATING_STUDENT = true;
    cursor -> row = new_student();
    cursor -> new_row = true;
    print_student_information(cursor -> row);
}

ExecutionResult execute_get(DataBase *db) {
    for (int i = 0; i < db -> nums_rows; i++)
        print_row(db -> rows[i]);
    return EXECUTION_SUCCEED;
}

ExecutionResult execute_select(DataBase *db, Cursor* cursor, Student *student) {
    Student *s;
    for (int i = 0; i < db -> nums_rows; i++){
        s = db -> rows[i];
        if (strcmp(student -> name, s -> name) == 0 && strcmp(student -> surname, s -> surname) == 0 &&
        strcmp(student -> patronymic, s -> patronymic) == 0){
            cursor -> row = s;
            cursor -> num_row = i+1;
            print_student_information(cursor -> row);
            return EXECUTION_SUCCEED;
        }
    }
    return EXECUTION_FAILED;
}

ExecutionResult execute_add(Statement *statement, Cursor *cursor, DataBase *db){
    Student *s = cursor -> row;
    if (strcmp(statement -> field, "exams") == 0){
        if (s -> exams_length < COLUMN_EXAMS_LENGTH) {
            Exam *e = statement -> value;
            s -> exams[s -> exams_length] = *e;
            s -> exams_length += 1;
            statement -> value = NULL;
            print_student_information(s);
            return EXECUTION_SUCCEED;
        }

    } else if (strcmp(statement -> field, "tests") == 0) {
        if (s -> tests_length < COLUMN_TESTS_LENGTH) {
            Test *e = statement -> value;
            printf("%s\n", e ->name);
            printf("%s\n", e ->date);
            s -> tests[s -> tests_length] = *e;
            s -> tests_length += 1;
            statement -> value = NULL;
            print_student_information(s);
            return EXECUTION_SUCCEED;
        }
    }
    return EXECUTION_FAILED;
}

ExecutionResult execute_remove(Statement *statement, Cursor *cursor, DataBase *db) {
    Student *s = cursor -> row;
    if (strcmp(statement -> field, "exams") == 0){
        for (int i = 0; i < COLUMN_EXAMS_LENGTH; i++){
            if (strcmp(s -> exams[i].name, statement -> value) == 0) {
                for (int j = i+1; j < COLUMN_EXAMS_LENGTH; j++)
                    s -> exams[j-1] = s -> exams[j];
                memset(&(s -> exams[s -> exams_length]), 0, COLUMN_EXAM_LENGTH);
                s -> exams_length -= 1;
                print_student_information(s);
                return EXECUTION_SUCCEED;
            }
        }
    }
    else if (strcmp(statement -> field, "tests") == 0){
        for (int i = 0; i < COLUMN_TESTS_LENGTH; i++){
            if (strcmp(s -> tests[i].name, statement -> value) == 0) {
                for (int j = i+1; j < COLUMN_TESTS_LENGTH; j++)
                    s -> tests[j-1] = s -> tests[j];
                memset(&(s -> exams[s -> tests_length]), 0, COLUMN_EXAM_LENGTH);
                s -> tests_length -= 1;
                print_student_information(s);
                return EXECUTION_SUCCEED;
            }
        }
    }
    return EXECUTION_FAILED;
}

ExecutionResult execute_delete(DataBase *db, Cursor *cursor) {
    delete_row(db, cursor -> num_row);
    db_save(db);
    EDITING_STUDENT = false;
    return EXECUTION_SUCCEED;
}

ExecutionResult execute_cancel(){
    CREATING_STUDENT = false;
    EDITING_STUDENT = false;
    printf("You have exited the user editing mode.\n");
}

ExecutionResult execute_sort(DataBase *db){
    Student *s1;
    Student *s2;
    for (int i = db ->nums_rows - 2; i >= 0; i--){
        for (int j = i; j < db -> nums_rows - 1; j++) {
            s1 = db ->rows[j];
            s2 = db -> rows[j+1];
            if (strcmp(s1 -> surname, s2 -> surname) > 0){
                void *t = db -> rows[j+1];
                db -> rows[j+1] = db -> rows[j];
                db -> rows[j] = t;
            }
        }
    }
    execute_get(db);
    return EXECUTION_SUCCEED;
}


PrepareStatement prepare_create_student(InputBuffer *input_buffer, Statement *statement){
    if (strncmp(input_buffer -> buffer, "cancel", 6) == 0){
        statement -> type = STATEMENT_CANCEL;
        return PREPARE_SUCCEED;
    }
    if (strncmp(input_buffer -> buffer, "confirm", 7) == 0) {
        statement -> type = STATEMENT_CONFIRM;
        return PREPARE_SUCCEED;
    }
    else {
        char *field = strtok(input_buffer->buffer, " ");
        if (strcmp(field, "tests") == 0) {
            char *command = strtok(NULL, " ");
            if (strcmp(command, "remove") == 0) {
                char *name = strtok(NULL, " ");
                statement->field = field;
                statement->value = name;
                statement->type = STATEMENT_REMOVE;
            } else {
                char *name = strtok(NULL, " ");
                char *date = strtok(NULL, " ");
                statement -> field = field;
                Test* a = new_test();
                strcpy(a -> name, name);
                strcpy(a -> date, date);
                statement->value = a;
                statement->type = STATEMENT_ADD;
            }
            return PREPARE_SUCCEED;
        }
        else if (strcmp(field, "exams") == 0){
            char *command = strtok(NULL, " ");
            if (strcmp(command, "remove") == 0) {
                char *name = strtok(NULL, " ");
                statement->field = field;
                statement->value = name;
                statement->type = STATEMENT_REMOVE;
            } else {
                char *name = strtok(NULL, " ");
                char *date = strtok(NULL, " ");
                char *mark = strtok(NULL, " ");
                statement->field = field;
                Exam *a = new_exam();
                strcpy(a -> name, name);
                strcpy(a -> date, date);
                a -> mark = (int)(mark[0] - '0');
                statement->value = a;
                statement->type = STATEMENT_ADD;
            }
            return PREPARE_SUCCEED;
        } else {
            char *command = strtok(NULL, " ");
            char *value = strtok(NULL, " ");

            if (command != NULL && strcmp(command, "set") == 0) {
                statement->type = STATEMENT_SET;
                statement -> field = field;
                statement -> value = value;
                return PREPARE_SUCCEED;
            }
        }
        return PREPARE_SYNTAX_ERROR;
    }
}

PrepareStatement prepare_edit_student(InputBuffer *input_buffer, Statement *statement) {
    if (strncmp(input_buffer -> buffer, "delete", 6) == 0){
        statement -> type = STATEMENT_DELETE;
        return PREPARE_SUCCEED;
    }
    return prepare_create_student(input_buffer, statement);
}

PrepareStatement prepare_append(Statement *statement){
    statement -> type = STATEMENT_APPEND;
    return PREPARE_SUCCEED;
}

PrepareStatement prepare_get(Statement *statement){
    statement -> type = STATEMENT_GET;
    return PREPARE_SUCCEED;
}

PrepareStatement prepare_sort(Statement *statement){
    statement -> type = STATEMENT_SORT;
    return PREPARE_SUCCEED;
}

PrepareStatement prepare_select(InputBuffer *input_buffer, Statement *statement) {
    char *command;
    char *name;
    char *surname;
    char *patronymic;

    command = strtok(input_buffer -> buffer, " ");
    name = strtok(NULL, " ");
    surname = strtok(NULL, " ");
    patronymic = strtok(NULL, " ");

    statement -> type = STATEMENT_SELECT;
    Student *s = new_student();
    strcpy(s -> name, name);
    strcpy(s -> surname, surname);
    strcpy(s -> patronymic, patronymic);
    statement -> value = s;
    EDITING_STUDENT = true;
    return PREPARE_SUCCEED;
}

PrepareStatement prepare_statement(InputBuffer *input_buffer, Cursor* cursor, Statement *statement) {
    if (CREATING_STUDENT)
        return prepare_create_student(input_buffer, statement);

    if (EDITING_STUDENT)
        return prepare_edit_student(input_buffer, statement);

    if (strncmp(input_buffer -> buffer, "append", 6) == 0)
        return prepare_append(statement);

    if (strncmp(input_buffer -> buffer, "get", 3) == 0)
        return prepare_get(statement);

    if (strncmp(input_buffer -> buffer, "select", 6) == 0)
        return prepare_select(input_buffer, statement);

    if (strncmp(input_buffer -> buffer, "sort", 4) == 0)
        return prepare_sort(statement);

    return PREPARE_UNRECOGNIZED_COMMAND;
}

ExecutionResult execute_statement(Statement *statement, Cursor *cursor, DataBase *db){
    switch (statement -> type) {
        case (STATEMENT_SET):
            return execute_set(cursor -> row, statement -> field, statement -> value);
        case (STATEMENT_CONFIRM):
            return execute_confirm(db, cursor);
        case (STATEMENT_SELECT):
            return execute_select(db, cursor, statement -> value);
        case (STATEMENT_APPEND):
            return execute_append(cursor);
        case (STATEMENT_DELETE):
            return execute_delete(db, cursor);
        case(STATEMENT_GET):
            return execute_get(db);
        case(STATEMENT_ADD):
            return execute_add(statement, cursor, db);
        case(STATEMENT_REMOVE):
            return execute_remove(statement, cursor, db);
        case (STATEMENT_CANCEL):
            return execute_cancel();
        case (STATEMENT_SORT):
            return execute_sort(db);
    }
}

void print_info(){

}


int main(int argc, char *argv[]) {
    InputBuffer *input_buffer = new_input_buffer();
    DataBase *db = NULL;
    Cursor *cursor = (Cursor*) malloc(sizeof(Cursor));
    while (true) {
        print_prompt();
        read_input(input_buffer);

        if (strncmp(input_buffer -> buffer, ".open", 5) == 0){
            char* filename;
            int var = sscanf(input_buffer ->buffer, ".open %s", filename);
            if (var < 1){
                printf("Filename is missing\n");
                continue;
            }
            else{
                db = db_open(filename);
                if (db == NULL)
                    printf("Unable to open file\n");
            }
            continue;
        }

        if (strcmp(input_buffer -> buffer, ".exit") == 0){
            db_close(db);
            exit(EXIT_SUCCESS);
        }

        if (db == NULL){
            printf("You need open database file\n");
            continue;
        }

        Statement statement;
        switch (prepare_statement(input_buffer, cursor, &statement)) {
            case (PREPARE_SUCCEED):
                break;
            case (PREPARE_UNRECOGNIZED_COMMAND):
                printf("Unrecognized command.\n");
                continue;
            case (PREPARE_SYNTAX_ERROR):
                printf("Syntax error.\n");
                continue;
        }

        switch (execute_statement(&statement, cursor, db)) {
            case (EXECUTION_SUCCEED):
                break;
            case (EXECUTION_FAILED):
                printf("An error occurred while executing the command.\n");
        }
    }
}
