// Copyright (C) 2017 Alexander Koch
#include "vector.h"

vector_t* vector_new(void) {
    vector_t* vector = malloc(sizeof(*vector));
    vector->capacity = VECTOR_CAPACITY;
    vector->size = 0;
    vector->data = malloc(sizeof(void*) * vector->capacity);
    return vector;
}

void vector_push(vector_t* vector, void* data) {
    if(vector->size == vector->capacity) {
        vector->capacity += VECTOR_CAPACITY;
        vector->data = realloc(vector->data, sizeof(void*) * vector->capacity);
        assert(vector->data != 0);
    }

    vector->data[vector->size++] = data;
}

void* vector_pop(vector_t* vector) {
    void* data = vector->data[vector->size--];
    if(vector->size == vector->capacity - VECTOR_CAPACITY) {
        vector->capacity -= VECTOR_CAPACITY;
        vector->data = realloc(vector->data, sizeof(void*) * vector->capacity);
        assert(vector->data != 0);
    }
    return data;
}

void* vector_top(vector_t* vector) {
    assert(vector->size > 0);
    return vector->data[vector->size-1];
}

void vector_set(vector_t* vector, size_t index, void* data) {
    if(index >= vector->size) {
        for(size_t i = vector->size; i < index; i++) {
            vector_push(vector, 0);
        }

        vector_push(vector, data);
        return;
    }

    assert(index < vector->size);
    vector->data[index] = data;
}

void* vector_get(vector_t* vector, size_t index) {
    assert(index < vector->capacity);
    return vector->data[index];
}

size_t vector_size(const vector_t* vector) {
    return vector->size;
}

void vector_free(vector_t* vector) {
    free(vector->data);
    free(vector);
}
