#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h> 


#define COLUMN_USERNAME_SIZE 32     //rozmiar w bajtach username
#define COLUMN_EMAIL_SIZE 255       //rozmiar w bajtach
#define size_of_attribute(Struct, Attribute) sizeof(((Struct*)0)->Attribute)        //rozmiar atrybutu, podajemy strukture, potem jego atrybut, 

typedef struct{
    //definicja struktury wiersza, trzyma id, username, emial
    uint32_t id;    // id w typie uint32_t || u-unsigned (nie przyjmuje ujemnych), t_32 
    char username[COLUMN_USERNAME_SIZE + 1];    //tablica na column_username 
    char email[COLUMN_EMAIL_SIZE +1];           // +1 ponieważ C zapisuje napisy dodajac do ich konca \0, wkazujac koniec tekstu
} Row;

const uint32_t ID_SIZE = size_of_attribute(Row, id);
const uint32_t USERNAME_SIZE = size_of_attribute(Row, username);
const uint32_t EMAIL_SIZE = size_of_attribute(Row, email);
const uint32_t ID_OFFSET = 0;
const uint32_t USERNAME_OFFSET = ID_OFFSET + ID_SIZE;
const uint32_t EMAIL_OFFSET = USERNAME_OFFSET + USERNAME_SIZE;
const uint32_t ROW_SIZE = ID_SIZE + USERNAME_SIZE + EMAIL_SIZE;

const uint32_t PAGE_SIZE = 4096;    //strona ma rozmiar 4096
#define TABLE_MAX_PAGES 100         // w tabeli max 100 stron
const uint32_t ROWS_PER_PAGE = PAGE_SIZE / ROW_SIZE;    //wiersze na strony = 4096 / 291
const uint32_t TABLE_MAX_ROWS = ROWS_PER_PAGE * TABLE_MAX_PAGES;

typedef struct{
    //(Zarządca I/O i Cache)

    int file_descriptor;    //uchwyt, funkcja otiwera plik funkja open() a ta zwraca liczbe calkowita, i nią bedziemy sie identyfikowac potem
    uint32_t file_length;   //dlugosc pliku w bajtach, pager nie wie co to tabele, wiersze ani jakie dane tam mamy, 
    void* pages[TABLE_MAX_PAGES];   //void* zwraca adres, mamy tu tablice adresow(100). Jest ta pamiec podreczna stron (Page cache)
} Pager;

typedef struct{
    //tabela nie posiada wlasnych danych, za to odpowiada pager, 

    uint32_t num_rows;  //liczba wierszy jakie przetrzymuje tablica
    Pager* pager;       //wskaznik do pagera
} Table;

typedef struct{
    Table* table;
    uint32_t row_num;
    bool end_of_table;
} Cursor;

Cursor* table_start(Table* table){
    Cursor* cursor = malloc(sizeof(Cursor));
    cursor->table = table;
    cursor->row_num=0;
    cursor->end_of_table = (table->num_rows == 0);

    return cursor;
}

Cursor* table_end(Table* table){
    Cursor* cursor = malloc(sizeof(Cursor));
    cursor->table = table;
    cursor->row_num = table->num_rows;
    cursor->end_of_table = true;

    return cursor;
}

typedef struct {
    //InputBuffer bedie korzystal z funkcji getline, wiec musi miec buffer, wielkosc buffer by nie przekroczyc rozmiaru, oraz sprawdzic ile wpisal uztyykownik

    char* buffer;         //wskazujemy poczatek bloku pamieci w RAM, nie tablica bo nie wiemy co wpisze uzytkownik
    size_t buffer_length; //wielkosc naszego buffer
    ssize_t input_length; //ile znakow wpisal uzytkownik ssize bo moze byc ujemna ilosc(blad)
} InputBuffer;

typedef enum {
    STATEMENT_INSERT,
    STATEMENT_SELECT
} StatementType;

typedef struct{
    //Statement(statemant, row) -> albo insert albo select, dane ktore wkladamy

    StatementType type;
    Row row_to_insert;
} Statement;

typedef enum{
    PREPARE_SUCCESS,
    PREPARE_UNRECOGNIZED_STATEMENT,
    PREPARE_NEGATIVE_ID,
    PREPARE_STRING_TOO_LONG,
    PREPARE_SYNTAX_ERROR,
} PrepareResult;

typedef enum { 
    EXECUTE_SUCCESS, 
    EXECUTE_TABLE_FULL 
} ExecuteResult;

typedef enum{
    META_COMMAND_SUCCESS,
    META_COMMAND_UNRECOGNIZED_COMMAND
} MetaCommandResult;

Pager* pager_open(const char* filename){
    //funkcja otwiera plik -> sprawdza dlugosc pliku -> inicializuje pager -> ustawia kazda strone na NULL

    int fd = open(filename,
                O_RDWR |      // Read/Write mode
                    O_CREAT,  // Create file if it does not exist
                S_IWUSR |     // User write permission
                    S_IRUSR   // User read permission
                );
    if(fd == -1){
        printf("Unable to open file\n");
    }
    off_t file_length = lseek(fd, 0, SEEK_END);
    Pager* pager = malloc(sizeof(Pager));
    pager->file_descriptor = fd;
    pager->file_length = file_length;

    for (uint32_t i=0; i<TABLE_MAX_PAGES; i++){
        pager->pages[i] = NULL;                 //Robimy tak bo po zainicjowaniu pamieci przez malloc pages[i] są pełne smieci, czyscimy pamiec
    }

    return pager;
}

Table* db_open(const char* filename){
    // Warstwa abstrakcji dla interfejsu uzytkownika.
    // 1. Zleca Pagerowi fizyczne otwarcie/stworzenie pliku na dysku.
    // 2. Na podstawie wielkosci pliku w bajtach wylicza, ile wierszy mielismy juz zapisanych.
    // 3. Alokuje pamiec na glowna strukture Table, zamyka w niej Pagera i zwraca gotowy silnik bazy.

    Pager* pager = pager_open(filename);
    uint32_t num_rows = pager->file_length/ROW_SIZE;

    Table* table = malloc(sizeof(Table));
    table->pager = pager;
    table->num_rows = num_rows;
    return table;
}


PrepareResult prepare_insert(InputBuffer* input_buffer, Statement* statement){
    //konwertujemy caly string na poszczegole typy (id, username, email)

    statement->type = STATEMENT_INSERT;

    char* keyword = strtok(input_buffer->buffer, " ");
    char* id_string = strtok(NULL, " ");
    char* username = strtok(NULL, " ");
    char* email = strtok(NULL, " ");

    if( keyword == NULL || id_string == NULL || username == NULL || email == NULL){
        return PREPARE_SYNTAX_ERROR;
    }

    int id = atoi(id_string); // asci to integer
    if (id < 0){
        return PREPARE_NEGATIVE_ID;
    }
    if (strlen(username) > COLUMN_USERNAME_SIZE){
        return PREPARE_STRING_TOO_LONG;
    }
    if (strlen(email) > COLUMN_EMAIL_SIZE) {
        return PREPARE_STRING_TOO_LONG;
    }

    statement->row_to_insert.id = id;
    strcpy(statement->row_to_insert.username, username);            //uzywamy strcpy bo nie da sie przypisac stringa w C, 
    strcpy(statement->row_to_insert.email, email);

    return PREPARE_SUCCESS;

};

PrepareResult prepare_statement(InputBuffer* input_buffer, Statement* statement) {
    if (strncmp(input_buffer->buffer, "insert", 6) == 0) {
        return prepare_insert(input_buffer, statement);
    }
    if (strcmp(input_buffer->buffer, "select") == 0) {
        statement->type = STATEMENT_SELECT;
        return PREPARE_SUCCESS;
    }

    return PREPARE_UNRECOGNIZED_STATEMENT;
}

void serialize_row(Row* source, void* destination){
    memcpy(destination + ID_OFFSET, &(source->id), ID_SIZE);
    strncpy(destination + USERNAME_OFFSET, source->username, USERNAME_SIZE);
    strncpy(destination + EMAIL_OFFSET, source->email, EMAIL_SIZE);
}

void deserialize_row(void* source, Row* destination){
    memcpy(&(destination->id), source + ID_OFFSET, ID_SIZE);
    memcpy(&(destination->username), source + USERNAME_OFFSET, USERNAME_SIZE);
    memcpy(&(destination->email), source + EMAIL_OFFSET, EMAIL_SIZE);
}

void* get_page(Pager* pager, uint32_t page_num){
    if(page_num > PAGE_SIZE){
        printf("Tried to fetch page number out of bounds. %d > %d\n", page_num, TABLE_MAX_PAGES);
        exit(EXIT_FAILURE);
    }
    if (pager->pages[page_num] == NULL){
        void* page = malloc(PAGE_SIZE);
        uint32_t num_pages = pager->file_length / PAGE_SIZE;

        if (pager->file_length % PAGE_SIZE){
            num_pages+=1;
        }

        if (page_num <= num_pages){
            lseek(pager->file_descriptor, page_num * PAGE_SIZE, SEEK_SET);
            ssize_t bytes_read = read(pager->file_descriptor, page, PAGE_SIZE);
            if (bytes_read == -1){
                printf("Error reading file: %d\n", errno);
                exit(EXIT_FAILURE);
            }
        }
        pager->pages[page_num] = page;
    }

    return pager->pages[page_num];
}

void* cursor_value(Cursor* cursor){
    uint32_t row_num = cursor->row_num;
    uint32_t page_num = row_num / ROWS_PER_PAGE;
    void* page = get_page(cursor->table->pager, page_num);
    uint32_t row_offset = row_num % ROWS_PER_PAGE;
    uint32_t byte_offset = row_offset * ROW_SIZE;
    return page + byte_offset;
}

void cursor_advance(Cursor* cursor){
    cursor->row_num += 1;
    if (cursor->row_num >= cursor->table->num_rows){
        cursor->end_of_table = true;
    }
}

void* pager_flush(Pager* pager, uint32_t page_num, uint32_t size){
    if (pager->pages[page_num] == NULL){
        printf("Tried to flush null page\n");
        exit(EXIT_FAILURE);
    }

    off_t offset = lseek(pager->file_descriptor, page_num *PAGE_SIZE, SEEK_SET);

    if (offset == -1){
        printf("Error seeking: %d\n", errno);
        exit(EXIT_FAILURE);
    }

    ssize_t bytes_written = write(pager->file_descriptor, pager->pages[page_num], size);

    if (bytes_written == -1){
        printf("Error writing: %d\n", errno);
        exit(EXIT_FAILURE);
    }
}   

void db_close(Table* table){
    Pager* pager = table->pager;
    uint32_t num_full_pages = table->num_rows / ROWS_PER_PAGE;
    for(uint32_t i=0; i< num_full_pages; i++){
        if(pager->pages[i] == NULL){
            continue;
        }
        pager_flush(pager, i, PAGE_SIZE);
        free(pager->pages[i]);
        pager->pages[i] = NULL; 
    }
    uint32_t num_additional_rows = table->num_rows % ROWS_PER_PAGE;
    if (num_additional_rows > 0){
        uint32_t page_num = num_full_pages;
        if(pager->pages[page_num] != NULL){
            pager_flush(pager, page_num, num_additional_rows * ROW_SIZE);
            free(pager->pages[page_num]);
            pager->pages[page_num] = NULL;
        }
    }

    int result = close(pager->file_descriptor);
    if (result == -1){
        printf("Error closing db file.\n");
        exit(EXIT_FAILURE);
    }
    for (uint32_t i=0; i<TABLE_MAX_PAGES; i++){
        void* page = pager->pages[i];
        if(page){
            free(page);
            pager->pages[i] = NULL;
        }
    }
    free(pager);
    free(table);
}

ExecuteResult execute_insert(Statement* statement, Table* table){
    if (table->num_rows >= TABLE_MAX_ROWS){
        return EXECUTE_TABLE_FULL;
    }

    Row* row_to_insert = &(statement->row_to_insert);
    Cursor* cursor = table_end(table);

    serialize_row(row_to_insert, cursor_value(cursor));
    table->num_rows +=1;
    free(cursor);

    return EXECUTE_SUCCESS;
}

void print_row(Row* row) {
    printf("(%d, %s, %s)\n", row->id, row->username, row->email);
}

ExecuteResult execute_select(Statement* statement, Table* table){
    Cursor* cursor = table_start(table);
    Row row;
    while (!(cursor->end_of_table)){
        deserialize_row(cursor_value(cursor), &row);
        print_row(&row);
        cursor_advance(cursor);
    }

    free(cursor);

    return EXECUTE_SUCCESS;
}

ExecuteResult execute_statement(Statement* statement, Table* table){
    switch (statement->type){
        case (STATEMENT_INSERT):
            return execute_insert(statement, table);
        case (STATEMENT_SELECT):
            return execute_select(statement, table);
    }
}

MetaCommandResult do_meta_command(InputBuffer* input_buffer, Table* table){
    if (strcmp(input_buffer->buffer, ".exit") == 0){
        db_close(table);
        exit(EXIT_SUCCESS);
    }else{
        return META_COMMAND_UNRECOGNIZED_COMMAND;
    }
}

InputBuffer* new_input_buffer() {
  InputBuffer* input_buffer = malloc(sizeof(InputBuffer));
  input_buffer->buffer = NULL;
  input_buffer->buffer_length = 0;
  input_buffer->input_length = 0;

  return input_buffer;
}

void print_prompt() { printf("db > "); }

void read_input(InputBuffer* input_buffer) {
  ssize_t bytes_read = getline(&(input_buffer->buffer), &(input_buffer->buffer_length), stdin);

  if (bytes_read <= 0) {
    printf("Error reading input\n");
    exit(EXIT_FAILURE);
  }

  // Ignore trailing newline
  input_buffer->input_length = bytes_read - 1;
  input_buffer->buffer[bytes_read - 1] = 0;
}

void close_input_buffer(InputBuffer* input_buffer) {
    free(input_buffer->buffer);
    free(input_buffer);
}

int main(int argc, char* argv[]) {
    if (argc <2){
        printf("Must supply a database filename.\n");
        exit(EXIT_FAILURE);
    }
    
    char* filename = argv[1];
    Table* table = db_open(filename);

    InputBuffer* input_buffer = new_input_buffer();
    while (true) {
        print_prompt();
        read_input(input_buffer);

        if (input_buffer->buffer[0] == '.'){
            switch (do_meta_command(input_buffer, table)){
                case(META_COMMAND_SUCCESS):
                    continue;
                case(META_COMMAND_UNRECOGNIZED_COMMAND):
                    printf("Unrrecognized command '%s'\n", input_buffer->buffer);
                    continue;
            }
        }

        Statement statement;
        switch (prepare_statement(input_buffer, &statement)){
            case(PREPARE_SUCCESS):
                break;
            case (PREPARE_STRING_TOO_LONG):
                printf("String is too long.\n");
                continue;
            case(PREPARE_SYNTAX_ERROR):
                printf("Syntax error. Could not parse statement.\n");
                continue;
            case(PREPARE_NEGATIVE_ID):
                printf("ID must be positive.\n");
                continue;
            case(PREPARE_UNRECOGNIZED_STATEMENT):
                printf("Unrecpgnized keyword at start of '%s'.\n", input_buffer->buffer);
                continue;
        }
        
        switch (execute_statement(&statement, table)){
            case(EXECUTE_SUCCESS):
                printf("Executed.\n");
                break;
            case(EXECUTE_TABLE_FULL):
                printf("Error: Table full.\n");
                break;
        }

}
}