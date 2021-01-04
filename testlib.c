#include<stdio.h>
#include<stdlib.h>
#include<string.h>

#include"list.h"
#include"hash.h"
#include"bitmap.h"
//#include"hex_dump.h"  //not needed for now

//#define DEBUG   //ui for terminal
#define MAXSIZE_DATA 100    //max slots for data
#define MAXSIZE_PARSER 1200  //no duplication

/*----------------start of datastructure for custom data saving----------------*/

typedef enum{bitmap_e, hash_e, list_e} type_t;

int is_fp_stdin = 0;
int running = 1;

typedef struct __data_t{
    char name[100];
    type_t type;
    void* data; //the data
    
    //for easier node control
    struct __data_t* next;
    struct __data_t* prev;
    int occupied;
} data_t;

//container/header of data_t
typedef struct{
    data_t data[MAXSIZE_DATA];
    data_t* next;
    data_t* end;
    size_t size;    //keep track of size
} control_t;

//connect all nodes
void init_control(control_t* control){
    for(int i = 0; i < MAXSIZE_DATA - 1; i++){    //data[99].next -> END
        control->data[i].next = &control->data[i + 1];
        control->data[i + 1].prev = &control->data[i];
    }
    control->next = control->data;
    control->end = &control->data[MAXSIZE_DATA - 1];
}
//free all data on quit
void destroy_all(control_t* control){
    for(int i = 0; i < MAXSIZE_DATA; i++){
        if(control->data[i].data){
#ifdef DEBUG
            if(is_fp_stdin){
                printf("freeing data: %p\n", control->data[i].data);
            }
#endif
            free(control->data[i].data);    //tombstone-like
        }
    }
}
data_t* get_last_empty(control_t* control){
    data_t* curr = control->next;
    while(curr->occupied && curr->next){
        curr = curr->next;
    }
    if(curr->occupied && !curr->next) return (void*)-1;   //no place to insert
    return curr;
}
data_t* get_data(control_t* control, char* name){
    data_t* curr = control->next;
    int found = 0;
    while(curr->occupied && curr->next){
        if(!strcmp(curr->name, name)){
            found = 1;
            break;
        }
        else curr = curr->next;
    }
    if(!found) return (void*)-1;   //data doesnt exist
    return curr;
}
void append_data(control_t* control, char* name, type_t type, size_t bit_cnt){
    if(control->size >= MAXSIZE_DATA) return;
    
    data_t* curr = get_last_empty(control);

    control->size++;
    curr->occupied = 1;
    
    strcpy(curr->name, name);
    curr->type = type;

    //>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>data initialization here
    switch(type){
    case bitmap_e:
        curr->data = bitmap_create(bit_cnt);
        break;
    case hash_e:
        curr->data = malloc(sizeof(struct hash));
        hash_init(curr->data, &hhf_custom, &hlf_custom, NULL);
        break;
    case list_e:
        curr->data = malloc(sizeof(struct list));
        list_init(curr->data);
        break;
    }
    //>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>data initialization here
#ifdef DEBUG   
    if(is_fp_stdin)
        printf("append_data: name = %s, type = %d, data = %p, next = %p, prev = %p, occupied = %d\n",
        curr->name, curr->type, curr->data, curr->next, curr->prev, curr->occupied);
#endif
}
void hide_data(control_t* control, char* name){
    data_t* curr = get_data(control, name);


    if(curr == (void*)-1){
        return; //no data
    }
#ifdef DEBUG
    else{
        if(is_fp_stdin)
            printf("hide_data: name = %s, type = %d, data = %p, next = %p, prev = %p, occupied = %d\n",
            curr->name, curr->type, curr->data, curr->next, curr->prev, curr->occupied);
    }
#endif
    

    control->size--;
    curr->occupied = 0;

    //clean up surroundings
    if(curr->prev){
        curr->prev->next = curr->next;
        curr->next->prev = curr->prev;
    }
    else{
        //if prev is a header
        control->next = curr->next;
        curr->next->prev = 0;
    }

    //move to the end of list
    curr->next = 0;
    curr->prev = control->next;
    control->end->next = curr;
    control->end = curr;

}

void printall(control_t* control){
    data_t* curr = control->next;
    printf("<");
    while(curr->occupied && curr->next){
        printf("%s, ", curr->name);
        curr = curr->next;
    }
    printf(">\n");
}

control_t control = {0};

/*----------------end of datastructure for custom data saving----------------*/

/*----------------start of datastructure for parsing----------------*/
typedef struct{
    void (*param_func)(int, char**);
    int param_count;
} parser_hash_elem;

typedef struct{
    parser_hash_elem parse[MAXSIZE_PARSER];
} parser_hash;

parser_hash _parser_hash = {0};

#define PARAM_CORE(PARAM0) \
        _parser_hash.parse[hash_string(#PARAM0) % MAXSIZE_PARSER]\

#define PARAM_CORE_NOSTRING(PARAM0) \
        _parser_hash.parse[hash_string(PARAM0) % MAXSIZE_PARSER]\

#define PARAM_FUNC_INIT(PARAM0) \
        PARAM_CORE(PARAM0).param_func = &PARAM0\

#define PARAM_COUNT_INIT(PARAM0) \
        PARAM_CORE(PARAM0).param_count\

#define PARAM_INIT(PARAM0)  \
        PARAM_FUNC_INIT(PARAM0);\
        PARAM_COUNT_INIT(PARAM0)\

void init_parser(); //explicit declaration

void parser(FILE* fp){

    //init
    init_control(&control);
    init_parser();


#ifdef DEBUG
    if(fp == stdin){
        printf("terminal mode!\n");
        is_fp_stdin = 1;    //print useful ui on stdin
    }
#endif

    char input[100];
    char* param[10];
    char param0[100];   //for param0 parsing
    while(running){
#ifdef DEBUG
        if(is_fp_stdin){
            printf("size = %ld ", control.size); printall(&control); printf("input>> ");
        }
#endif
        fgets(input, 100, fp);
        if(strlen(input)!= 0) input[strlen(input) - 1] = '\0';    //remove trailing newline
        
        memset(param, 0, sizeof(char*) * 10);
        param[0] = strtok(input, " ");
        int param_count = 1;
        for(; (param_count < 10) && (param[param_count] = strtok(NULL, " ")); param_count++);

        if(!param[0]) continue; //skip

        //little edit
        strcpy(param0, "parse_");
        strcat(param0, param[0]);

        //execute
        if(PARAM_CORE_NOSTRING(param0).param_func)
            PARAM_CORE_NOSTRING(param0).param_func(param_count, param);
    }

}

/*----------------end of datastructure for parsing----------------*/

void parse_create(int param_count, char** param){
    if(!strcmp(param[1], "list") && (param_count > 2)){
        append_data(&control, param[2], list_e, 0);

    }
    else if(!strcmp(param[1], "hashtable") && (param_count > 2)){
        append_data(&control, param[2], hash_e, 0);

    }
    else if(!strcmp(param[1], "bitmap") && (param_count > 3)){
        append_data(&control, param[2], bitmap_e, atoi(param[3]));
    }
}
void parse_delete(int param_count, char** param){
    hide_data(&control, param[1]);
}
void parse_dumpdata(int param_count, char** param){
    //dumpdata ds
    struct bitmap* bitmap_v;
    struct hash* hash_v;
    struct hash_iterator hash_i;
    struct list* list_v;
    struct list_elem* list_elem_v;
    struct list_item* list_item_v;

    data_t* curr = get_data(&control, param[1]);
    if(curr == (void*)-1) return;

    //trigger newline
    int nlflag = 0;

    switch(curr->type){
    case bitmap_e:

        bitmap_v = curr->data;
        int bit_size = bitmap_size(bitmap_v);
        for(int i = 0; i < bit_size; i++, nlflag = 1)
            printf("%d", bitmap_test(bitmap_v, i));
        
        if(nlflag) printf("\n");
        
        break;
    case hash_e:
        hash_v = curr->data;
        
        //loop each bucket
        for(int i = 0; i < hash_v->bucket_cnt; i++){
            list_v = &hash_v->buckets[i];
            list_elem_v = list_begin(list_v);

            //each bucket(list) - same as list dumpdata
            while(list_elem_v->next){
                nlflag = 1;
                list_item_v = list_entry(list_elem_v, struct list_item, elem);
                printf("%d ", list_item_v->data);
                list_elem_v = list_next(list_elem_v);
            }
        }

        if(nlflag) printf("\n");

        break;
    case list_e:

        list_v = curr->data;
        list_elem_v = list_begin(list_v);

        //cant use some built ins due to assertion code :(
        while(list_elem_v->next){
            nlflag = 1;
            list_item_v = list_entry(list_elem_v, struct list_item, elem);
            printf("%d ", list_item_v->data);
            list_elem_v = list_next(list_elem_v);
        }
        
        if(nlflag) printf("\n");

        break;
    }
}
//bitmap
void parse_bitmap_all(int param_count, char** param){
    //bool
    printf("%s\n",
    bitmap_all(get_data(&control, param[1])->data, atoi(param[2]), atoi(param[3]))
    ? "true":"false");

}
void parse_bitmap_any(int param_count, char** param){
    //bool
    printf("%s\n",
    bitmap_any(get_data(&control, param[1])->data, atoi(param[2]), atoi(param[3]))
    ? "true":"false");
}
void parse_bitmap_contains(int param_count, char** param){
    //bool
    printf("%s\n",
    bitmap_contains(get_data(&control, param[1])->data, atoi(param[2]), atoi(param[3]),
    (!strcmp(param[4], "true")) ? true : false)
    ? "true":"false");

}
void parse_bitmap_count(int param_count, char** param){
    //size_t
    printf("%u\n",
    bitmap_count(get_data(&control, param[1])->data, atoi(param[2]), atoi(param[3]),
    (!strcmp(param[4], "true")) ? true : false)
    );

}
void parse_bitmap_dump(int param_count, char** param){
    bitmap_dump(get_data(&control, param[1])->data);

}
void parse_bitmap_expand(int param_count, char** param){
    //struct bitmap*
    bitmap_expand(get_data(&control, param[1])->data, atoi(param[2]));

}
void parse_bitmap_flip(int param_count, char** param){
    bitmap_flip(get_data(&control, param[1])->data, atoi(param[2]));

}
void parse_bitmap_mark(int param_count, char** param){
    bitmap_mark(get_data(&control, param[1])->data, atoi(param[2]));

}
void parse_bitmap_none(int param_count, char** param){
    //bool
    printf("%s\n",
    bitmap_none(get_data(&control, param[1])->data, atoi(param[2]), atoi(param[3]))
    ? "true":"false");

}
void parse_bitmap_reset(int param_count, char** param){
    bitmap_reset(get_data(&control, param[1])->data, atoi(param[2]));

}
void parse_bitmap_scan(int param_count, char** param){
    //size_t
    printf("%u\n",
    bitmap_scan(get_data(&control, param[1])->data, atoi(param[2]), atoi(param[3]),
    (!strcmp(param[4], "true")) ? true : false)
    );

}
void parse_bitmap_scan_and_flip(int param_count, char** param){
    //size_t
    printf("%u\n",
    bitmap_scan_and_flip(get_data(&control, param[1])->data, atoi(param[2]), atoi(param[3]),
    (!strcmp(param[4], "true")) ? true : false)
    );

}
void parse_bitmap_set(int param_count, char** param){
    bitmap_set(get_data(&control, param[1])->data, atoi(param[2]),
    (!strcmp(param[3], "true")) ? true : false);

}
void parse_bitmap_set_all(int param_count, char** param){
    bitmap_set_all(get_data(&control, param[1])->data,
    (!strcmp(param[2], "true")) ? true : false);

}
void parse_bitmap_set_multiple(int param_count, char** param){
    bitmap_set_multiple(get_data(&control, param[1])->data, atoi(param[2]), atoi(param[3]),
    (!strcmp(param[4], "true")) ? true : false);

}
void parse_bitmap_size(int param_count, char** param){
    //size_t
    printf("%u\n",
    bitmap_size(get_data(&control, param[1])->data)
    );

}
void parse_bitmap_test(int param_count, char** param){
    //bool
    printf("%s\n",
    bitmap_test(get_data(&control, param[1])->data, atoi(param[2]))
    ? "true":"false");

}
//hash
void parse_hash_apply(int param_count, char** param){
    struct hash* hash_v = get_data(&control, param[1])->data;
    
    //pass aux
    int tempaux = 0;
    if(!strcmp("square", param[2])) tempaux = 0;    //square
    else if(!strcmp("triple", param[2])) tempaux = 1;   //triple
    hash_v->aux = &tempaux;

    hash_apply(hash_v, &haf_custom);
}
void parse_hash_insert(int param_count, char** param){
    struct hash_elem* elem = malloc(sizeof(struct hash_elem));
    struct list_item* item = list_entry(&elem->list_elem, struct list_item, elem);
    item->data = atoi(param[2]);

    hash_insert(get_data(&control, param[1])->data, elem);
}
void parse_hash_delete(int param_count, char** param){
    struct hash_elem* elem = malloc(sizeof(struct hash_elem));
    struct list_item* item = list_entry(&elem->list_elem, struct list_item, elem);
    item->data = atoi(param[2]);

    hash_delete(get_data(&control, param[1])->data, elem);
}
void parse_hash_find(int param_count, char** param){
    struct hash_elem* elem = malloc(sizeof(struct hash_elem));
    struct list_item* item = list_entry(&elem->list_elem, struct list_item, elem);
    item->data = atoi(param[2]);

    struct hash_elem* result = hash_find(get_data(&control, param[1])->data, elem);
    struct list_item* res_item = list_entry(&result->list_elem, struct list_item, elem);
    if(res_item) printf("%d\n", res_item->data);
}
void parse_hash_replace(int param_count, char** param){
    struct hash_elem* elem = malloc(sizeof(struct hash_elem));
    struct list_item* item = list_entry(&elem->list_elem, struct list_item, elem);
    item->data = atoi(param[2]);

    hash_replace(get_data(&control, param[1])->data, elem);
}
void parse_hash_empty(int param_count, char** param){
    //bool
    printf("%s\n",
    hash_empty(get_data(&control, param[1])->data)
    ? "true":"false");

}
void parse_hash_size(int param_count, char** param){
    //size_t
    printf("%u\n",
    hash_size(get_data(&control, param[1])->data)
    );
}
void parse_hash_clear(int param_count, char** param){
    struct hash* hash_v = get_data(&control, param[1])->data;
    
    //pass aux
    int tempaux = 2;    //destructor
    hash_v->aux = &tempaux;

    hash_clear(hash_v, &haf_custom);
}
//list
void parse_list_push_back(int param_count, char** param){
    struct list_elem* elem = malloc(sizeof(struct list_elem));
    struct list_item* item = list_entry(elem, struct list_item, elem);
    item->data = atoi(param[2]);
    list_push_back(get_data(&control, param[1])->data, elem);
}
void parse_list_push_front(int param_count, char** param){
    struct list_elem* elem = malloc(sizeof(struct list_elem));
    struct list_item* item = list_entry(elem, struct list_item, elem);
    item->data = atoi(param[2]);
    list_push_front(get_data(&control, param[1])->data, elem);
}
void parse_list_front(int param_count, char** param){
    struct list_elem* elem = list_front(get_data(&control, param[1])->data);
    struct list_item* item = list_entry(elem, struct list_item, elem);
    printf("%d\n", item->data);
}
void parse_list_back(int param_count, char** param){
    struct list_elem* elem = list_back(get_data(&control, param[1])->data);
    struct list_item* item = list_entry(elem, struct list_item, elem);
    printf("%d\n", item->data);
}
void parse_list_pop_back(int param_count, char** param){
    struct list_elem* elem = list_pop_back(get_data(&control, param[1])->data);
    struct list_item* item = list_entry(elem, struct list_item, elem);
}
void parse_list_pop_front(int param_count, char** param){
    struct list_elem* elem = list_pop_front(get_data(&control, param[1])->data);
    struct list_item* item = list_entry(elem, struct list_item, elem);
}
void parse_list_insert_ordered(int param_count, char** param){
    struct list_elem* elem = malloc(sizeof(struct list_elem));
    struct list_item* item = list_entry(elem, struct list_item, elem);
    item->data = atoi(param[2]);
    list_insert_ordered(get_data(&control, param[1])->data, elem, &llf_custom, NULL);
}
void parse_list_insert(int param_count, char** param){
    //find nth place elem
    struct list* list_v = get_data(&control, param[1])->data;
    struct list_elem* list_elem_v = list_begin(list_v);
    int nth = atoi(param[2]);
    for(int i = 0; i < nth; i++)
        list_elem_v = list_next(list_elem_v);
    
    //insert before the nth
    struct list_elem* elem = malloc(sizeof(struct list_elem));
    struct list_item* item = list_entry(elem, struct list_item, elem);
    item->data = atoi(param[3]);
    list_insert(list_elem_v, elem);
}
void parse_list_empty(int param_count, char** param){
    //bool
    printf("%s\n",
    list_empty(get_data(&control, param[1])->data)
    ? "true":"false");
}
void parse_list_size(int param_count, char** param){
    //size_t
    printf("%u\n",
    list_size(get_data(&control, param[1])->data)
    );
}
void parse_list_max(int param_count, char** param){
    struct list_elem* elem = list_max(get_data(&control, param[1])->data, &llf_custom, NULL);
    struct list_item* item = list_entry(elem, struct list_item, elem);
    printf("%d\n", item->data);

}
void parse_list_min(int param_count, char** param){
    struct list_elem* elem = list_min(get_data(&control, param[1])->data, &llf_custom, NULL);
    struct list_item* item = list_entry(elem, struct list_item, elem);
    printf("%d\n", item->data);
}
void parse_list_remove(int param_count, char** param){
    //find nth place elem
    struct list* list_v = get_data(&control, param[1])->data;
    struct list_elem* list_elem_v = list_begin(list_v);
    int nth = atoi(param[2]);
    for(int i = 0; i < nth; i++)
        list_elem_v = list_next(list_elem_v);

    //remove before the nth
    list_remove(list_elem_v);
}
void parse_list_reverse(int param_count, char** param){
    list_reverse(get_data(&control, param[1])->data);
}
void parse_list_shuffle(int param_count, char** param){
    list_shuffle(get_data(&control, param[1])->data);
}
void parse_list_swap(int param_count, char** param){
    //find nth place elem
    struct list* list_v = get_data(&control, param[1])->data;

    struct list_elem* list_elem_v = list_begin(list_v);
    int nth = atoi(param[2]);
    for(int i = 0; i < nth; i++)
        list_elem_v = list_next(list_elem_v);

    //find another place to swap
    struct list_elem* elem = list_begin(list_v);
    nth = atoi(param[3]);
    for(int i = 0; i < nth; i++)
        elem = list_next(elem);

    list_swap(list_elem_v, elem);
}
void parse_list_sort(int param_count, char** param){
    list_sort(get_data(&control, param[1])->data, &llf_custom, NULL);
}
void parse_list_splice(int param_count, char** param){
    struct list* list_v = get_data(&control, param[1])->data;

    struct list_elem* list_elem_v = list_begin(list_v);
    int nth = atoi(param[2]);
    for(int i = 0; i < nth; i++)
        list_elem_v = list_next(list_elem_v);

    //another list to splice from
    list_v = get_data(&control, param[3])->data;
    struct list_elem* start = list_begin(list_v);
    nth = atoi(param[4]);
    for(int i = 0; i < nth; i++)
        start = list_next(start);

    struct list_elem* end = list_begin(list_v);
    nth = atoi(param[5]);
    for(int i = 0; i < nth; i++)
        end = list_next(end);

    list_splice(list_elem_v, start, end);

}
void parse_list_unique(int param_count, char** param){
    struct list* list_v = NULL;
    if(param_count > 2){    //list_unique can have 1 or 2 params
        list_v = get_data(&control, param[2])->data;
        if(list_v == (void*)-1) list_v = NULL;
    }
    list_unique(get_data(&control, param[1])->data, list_v, &llf_custom, NULL);
}
void parse_quit(int param_count, char** param){
    destroy_all(&control);
    running = 0;
    return;
}

void init_parser(){
    PARAM_INIT(parse_create) = 2;
    PARAM_INIT(parse_delete) = 1;
    PARAM_INIT(parse_dumpdata) = 1;
    PARAM_INIT(parse_bitmap_all) = 3;
    PARAM_INIT(parse_bitmap_any) = 3;
    PARAM_INIT(parse_bitmap_contains) = 4;
    PARAM_INIT(parse_bitmap_count) = 4;
    PARAM_INIT(parse_bitmap_dump) = 1;
    PARAM_INIT(parse_bitmap_expand) = 2;
    PARAM_INIT(parse_bitmap_flip) = 2;
    PARAM_INIT(parse_bitmap_mark) = 2;
    PARAM_INIT(parse_bitmap_none) = 3;
    PARAM_INIT(parse_bitmap_reset) = 2;
    PARAM_INIT(parse_bitmap_scan) = 4;
    PARAM_INIT(parse_bitmap_scan_and_flip) = 4;
    PARAM_INIT(parse_bitmap_set) = 3;
    PARAM_INIT(parse_bitmap_set_all) = 2;
    PARAM_INIT(parse_bitmap_set_multiple) = 4;
    PARAM_INIT(parse_bitmap_size) = 1;
    PARAM_INIT(parse_bitmap_test) = 2;
    PARAM_INIT(parse_hash_apply) = 2;
    PARAM_INIT(parse_hash_insert) = 2;
    PARAM_INIT(parse_hash_delete) = 2;
    PARAM_INIT(parse_hash_find) = 2;
    PARAM_INIT(parse_hash_replace) = 2;
    PARAM_INIT(parse_hash_empty) = 1;
    PARAM_INIT(parse_hash_size) = 1;
    PARAM_INIT(parse_hash_clear) = 1;
    PARAM_INIT(parse_list_push_back) = 2;
    PARAM_INIT(parse_list_push_front) = 2;
    PARAM_INIT(parse_list_front) = 1;
    PARAM_INIT(parse_list_back) = 1;
    PARAM_INIT(parse_list_pop_back) = 1;
    PARAM_INIT(parse_list_pop_front) = 1;
    PARAM_INIT(parse_list_insert_ordered) = 2;
    PARAM_INIT(parse_list_insert) = 3;
    PARAM_INIT(parse_list_empty) = 1;
    PARAM_INIT(parse_list_size) = 1;
    PARAM_INIT(parse_list_max) = 1;
    PARAM_INIT(parse_list_min) = 1;
    PARAM_INIT(parse_list_remove) = 2;
    PARAM_INIT(parse_list_reverse) = 1;
    PARAM_INIT(parse_list_shuffle) = 1;
    PARAM_INIT(parse_list_swap) = 3;
    PARAM_INIT(parse_list_sort) = 1;
    PARAM_INIT(parse_list_splice) = 5;
    PARAM_INIT(parse_list_unique) = 1;
    PARAM_INIT(parse_quit) = 0;
}

int main(int argc, char* argv[]){
    

    if(argc > 1){   //.in file
        FILE* fp = fopen(argv[1], "r");
        parser(fp);
        fclose(fp);
    }
    else{
        parser(stdin); //terminal mode
    }
    return 0;
}