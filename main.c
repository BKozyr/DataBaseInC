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

typedef enum {NODE_INTERNAL, NODE_LEAF} NodeType;

/*
* Common Node Header Layout
*/

const uint32_t NODE_TYPE_SIZE = sizeof(uint8_t);        //1 bajt, typ node (LEAF albo INTERNAL)
const uint32_t NODE_TYPE_OFFSET = 0;                    //offset - przesuniecie, od ktorego moemntu zapisujemy typ node
const uint32_t IS_ROOT_SIZE = sizeof(uint8_t);          // flaga, root czy nie (0,1)
const uint32_t IS_ROOT_OFFSET = NODE_TYPE_SIZE;         // gdzie kladziemy te flage -> za typem node
const uint32_t PARENT_POINTER_SIZE = sizeof(uint32_t);  //numer strony rodzica
const uint32_t PARENT_POINTER_OFFSET = IS_ROOT_OFFSET + IS_ROOT_SIZE;
const uint32_t COMMON_NODE_HEADER_SIZE = NODE_TYPE_SIZE + IS_ROOT_SIZE + PARENT_POINTER_SIZE; // 1+1+4 = 6

/*
* Leaf Node Header Layout dodatkowy naglowek, tylko dla liscia
*/

const uint32_t LEAF_NODE_NUM_CELLS_SIZE = sizeof(uint32_t);                 //ilość wierszy
const uint32_t LEAF_NODE_NUM_CELLS_OFFSET = COMMON_NODE_HEADER_SIZE;        //gdzie zaczynamy to zapisywac -> za headerem
const uint32_t LEAF_NODE_HEADER_SIZE = LEAF_NODE_NUM_CELLS_SIZE + LEAF_NODE_NUM_CELLS_OFFSET;   // 4 + 6 = 10 10 bajtow na administracje

/*
* Leaf Node Body Layout     magazanyn na dane- od 10 bajtu do konca strony(4096)
*/

const uint32_t LEAF_NODE_KEY_SIZE = sizeof(uint32_t);       //4 bajty na klucz komorki (nasze id)
const uint32_t LEAF_NODE_KEY_OFFSET = 0;                    //offset
const uint32_t LEAF_NODE_VALUE_SIZE = ROW_SIZE;             //wielosc wiersza, 293 bajty (username, emial)
const uint32_t LEAF_NODE_VALUE_OFFSET = LEAF_NODE_KEY_OFFSET + LEAF_NODE_KEY_SIZE;  //dane leza tuz za kluczem
const uint32_t LEAF_NODE_CELL_SIZE = LEAF_NODE_KEY_SIZE + LEAF_NODE_VALUE_SIZE;     // wielosc wiersza klucz + wartosc 4 + 293 = 297
const uint32_t LEAF_NODE_SPACE_FOR_CELLS = PAGE_SIZE - LEAF_NODE_HEADER_SIZE;       // 4096 - 10 bajtow miejsca na komorki
const uint32_t LEAF_NODE_MAX_CELLS = LEAF_NODE_SPACE_FOR_CELLS / LEAF_NODE_CELL_SIZE;   //4086 / 297 = 13 weirszy zmiesic sie na jednej stronie

/*
Funkcje zwracające adresy pamięci kluczy, wartosci, metadanych
Przez to ze są to adersy, mogą byc zarówno
getterami jak i setterami
*/

uint32_t* leaf_node_num_cells(void* node){
    return node + LEAF_NODE_NUM_CELLS_OFFSET;
}

void* leaf_node_cell(void* node, uint32_t* cell_num){
    return node + LEAF_NODE_HEADER_SIZE + cell_num * LEAF_NODE_CELL_SIZE;
}

uint32_t* leaf_node_key(void* node, uint32_t cell_num){
    return leaf_node_cell(node, cell_num) + LEAF_NODE_KEY_OFFSET;
}

void* leaf_node_value(void* node, uint32_t cell_num){
    return  leaf_node_cell(node, cell_num) + LEAF_NODE_VALUE_OFFSET;
}

void initialize_leaf_node(void* node) {
    *leaf_node_num_cells(node) = 0; 
}

void print_constants() {
    printf("ROW_SIZE: %d\n", ROW_SIZE);
    printf("COMMON_NODE_HEADER_SIZE: %d\n", COMMON_NODE_HEADER_SIZE);
    printf("LEAF_NODE_HEADER_SIZE: %d\n", LEAF_NODE_HEADER_SIZE);
    printf("LEAF_NODE_CELL_SIZE: %d\n", LEAF_NODE_CELL_SIZE);
    printf("LEAF_NODE_SPACE_FOR_CELLS: %d\n", LEAF_NODE_SPACE_FOR_CELLS);
    printf("LEAF_NODE_MAX_CELLS: %d\n", LEAF_NODE_MAX_CELLS);
}

typedef struct{
    //(Zarządca I/O i Cache)

    int file_descriptor;    //uchwyt, funkcja otiwera plik funkja open() a ta zwraca liczbe calkowita, i nią bedziemy sie identyfikowac potem
    uint32_t file_length;   //dlugosc pliku w bajtach, pager nie wie co to tabele, wiersze ani jakie dane tam mamy, 
    uint32_t num_pages;     // ilosc stron
    void* pages[TABLE_MAX_PAGES];   //void* zwraca adres, mamy tu tablice adresow(100). Jest ta pamiec podreczna stron (Page cache)
} Pager;

typedef struct{
    //tabela nie posiada wlasnych danych, za to odpowiada pager, 
    Pager* pager;       //wskaznik do pagera
    uint32_t root_page_num;     //TERAZ tabela trzyma swoj root, kazdy select bedzie od tego ywchodzil
} Table;

typedef struct{
    Table* table;
    uint32_t page_num;
    uint32_t cell_num;  //zmiana z row -> cell_num nie przekroczy 13(LEAF_NODE_MAX_CELLS), gdy row mogl isc w nieskonczonosc
    bool end_of_table;
} Cursor;

Cursor* table_start(Table* table){
    //funkcja jest wywoływana jak uzytkownik wpisze select,
    // nasz kursor ustaiwa sie na pierwszym rekordzie.
    Cursor* cursor = malloc(sizeof(Cursor));
    cursor->table = table;
    //wspolrzedne kursora, [strona][numer wiersza]
    cursor->page_num = table->root_page_num;
    cursor->cell_num=0;

    void* root_node = get_page(table->pager, table->root_page_num);

    //sprawdzamy czy baza nie jest pusta
    cursor->end_of_table = (table->cell_rows == 13);
    uint32_t num_cells = *leaf_node_num_cells(root_node);
    cursor->end_of_table = (num_cells == 0);

    return cursor;
}

Cursor* table_end(Table* table){
    Cursor* cursor = malloc(sizeof(Cursor));
    cursor->table = table;
    cursor->page_num = table->root_page_num;
    
    void* root_node = get_page(table->pager, table->root_page_num);
    uint32_t num_cells = *leaf_node_num_cells(root_node);
    cursor-> cell_num = num_cells;
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

    pager->num_pages = file_length / PAGE_SIZE;
    if (file_length % PAGE_SIZE != 0){
        printf("Db file is not a whole number of pages. Corrupt file.\n");
        exit(EXIT_FAILURE);
    }

    for (uint32_t i=0; i<TABLE_MAX_PAGES; i++){
        pager->pages[i] = NULL;                 //Robimy tak bo po zainicjowaniu pamieci przez malloc pages[i] są pełne smieci, czyscimy pamiec
    }

    return pager;
}

Table* db_open(const char* filename){
    // Warstwa abstrakcji dla interfejsu uzytkownika.
    // 1. Zleca Pagerowi fizyczne otwarcie/stworzenie pliku na dysku.
    // NIEAKTUALNE  2. Na podstawie wielkosci pliku w bajtach wylicza, ile wierszy mielismy juz zapisanych.
    // 2. ustawia root_page na 0, nasz trzon drzewa, jezeli baza jest pusta, inicjalizujemy 1 lisc
    // 3. Alokuje pamiec na glowna strukture Table, zamyka w niej Pagera i zwraca gotowy silnik bazy.

    Pager* pager = pager_open(filename);
    Table* table = malloc(sizeof(Table));
    table->pager = pager;

    table->root_page_num = 0;
    if (pager->num_pages == 0){
        // nowy plik bazy danych. numer 0 to leaf_node
        void* root_node = get_page(pager,0);
        initialize_leaf_node(root_node);
    }

    return table;
}

void print_leaf_node(void* node){
    uint32_t num_cells = *leaf_node_num_cells(node);
    printf("Leaf (size: %d)\n", num_cells);
    for (uint32_t i =0; o< num_cells; i++){
        uint32_t key = *leaf_node_key(node, i);
        printf("  - %d : %d\n", i, key);
    } 
}

void leaf_node_insert(Cursor* cursor,uint32_t key, Row value){
    void* node = get_page(cursor->table->pager, cursor->page_num);  //adres do strony gdzie bedziemy wpisywac nowy wiersz

    uint32_t num_cells = *leaf_node_num_cells(node);    //aktualna liczba komorek(wierszy) na stronie
    if (num_cells>=LEAF_NODE_MAX_CELLS){
        //Node full
        printf("Need to implement splitting a leaf node.\n");
        exit(EXIT_FAILURE);
    }

    //sytuacja gdy mamy np: 1,5,10 chcemy wstawic id 4
    //wtedy przesuwamy indeksy robiac miejsce na id=4
    // 1,_,5,10
    if (cursor->cell_num < num_cells){
        //Make room for new cell
        for (uint32_t i = num_cells; i> cursor->cell_num; i--){
            memcpy(leaf_node_cell(node, i), leaf_node_cell(node,i -1), LEAF_NODE_CELL_SIZE);
        }
    }

    //wstawiamy wartosc, zwiekszamy liczbe wierszy(cells)
    *(leaf_node_num_cells(node)) += 1;
    *(leaf_node_key(node,cursor->cell_num)) = key;
    serialize_row(value, leaf_node_value(node, cursor->cell_num));

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

        if (page_num >= pager->num_pages){
            pager->num_pages = page_num +1;
        }
    }

    return pager->pages[page_num];
}

void* cursor_value(Cursor* cursor){
    uint32_t page_num = cursor->page_num;
    void* page = get_page(cursor->table->pager, page_num);
    return leaf_node_value(page, cursor->cell_num);
}

void cursor_advance(Cursor* cursor){
    uint32_t page_num = cursor->page_num;
    void* node = get_page(cursor->table->pager, page_num);

    cursor->cell_num +=1;
    if(cursor->cell_num >= (*leaf_node_num_cells(node))){
        cursor->end_of_table = true;
    }
}



void* pager_flush(Pager* pager, uint32_t page_num){
    // funkcja zapisuje dane do bazy danych, strona po stronie (petla w db_close)

    if (pager->pages[page_num] == NULL){
        printf("Tried to flush null page\n");
        exit(EXIT_FAILURE);
    }

    off_t offset = lseek(pager->file_descriptor, page_num *PAGE_SIZE, SEEK_SET);

    if (offset == -1){
        printf("Error seeking: %d\n", errno);
        exit(EXIT_FAILURE);
    }

    ssize_t bytes_written = write(pager->file_descriptor, pager->pages[page_num], PAGE_SIZE);

    if (bytes_written == -1){
        printf("Error writing: %d\n", errno);
        exit(EXIT_FAILURE);
    }
}   

void db_close(Table* table){
    Pager* pager = table->pager;

    for(uint32_t i=0; i< pager->num_pages; i++){
        if(pager->pages[i] == NULL){
            continue;
        }
        pager_flush(pager, i);
        free(pager->pages[i]);
        pager->pages[i] = NULL; 
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
    void* node = get_page(table->pager, table->root_page_num);
    if((*leaf_node_num_cells(node) >= LEAF_NODE_MAX_CELLS)){
        return EXECUTE_TABLE_FULL;
    }

    Row* row_to_insert = &(statement->row_to_insert);
    Cursor* cursor = table_end(table);

    leaf_node_insert(cursor, row_to_insert->id, row_to_insert);
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
    }else if(strcmp(input_buffer->buffer, ".constants") == 0){
        priintf("Constants:\n");
        print_constants();
        return META_COMMAND_SUCCESS;
    }   
    else if (strcmp(input_buffer->buffer, ".btree") == 0) {
        printf("Tree:\n");
        print_leaf_node(get_page(table->pager, 0));
        return META_COMMAND_SUCCESS;
    }
    else{
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