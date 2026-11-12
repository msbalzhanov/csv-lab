#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <math.h>
#include "cli.h"
#include "types.h"

// удаляет "/n"
static void delete_end(char *read)
{
    for(int i = 0; read[i] != 0; i++)
    {
        if(read[i] == '\n')
        {    
            read[i] = '\0';
            break;
        }
    }
}

static char *copy_str(const char *read)
{
    int length = 0;
    while(read[length] != 0)
        length++;

    char *copy_capacity = malloc(length + 1);
    if (copy_capacity == NULL)
        return NULL;

    for(int i = 0; i <= length; i++)
    {
        copy_capacity[i] = read[i];
    }

    return copy_capacity;
}

// копирует n символов
static char *copy_n(const char *s, int length)
{
    char *copy = malloc(length + 1);
    if (copy == NULL)
        return NULL;
    memcpy(copy, s, length);
    copy[length] = '\0';
    return copy;
}

// делит строку по запятым, пустые поля сохраняются,
static void split_line(const char *line, char **fields, int cols)
{
    const char *start = line;
    int index = 0;

    while (index < cols)
    {
        const char *comma = strchr(start, ',');
        int length = 0;
        if (comma != NULL) 
            length = (int)(comma - start);
        else 
            length = (int)strlen(start);

        fields[index++] = copy_n(start, length);
        if (comma == NULL)
            break;
        start = comma + 1;
    }
    while (index < cols)
        fields[index++] = copy_n("", 0);
}

static int is_int(const char *read)
{
    char *end;
    errno = 0;
    long check_int = strtol(read, &end, 10);
    if(end == read || *end != '\0' || errno == ERANGE)
        return 0; //false
    else    
        return 1; //true
}

static int is_double(const char *read)
{
    char *end;
    errno = 0;
    double check_double = strtod(read, &end);
    if(end != read && *end == '\0' && errno != ERANGE)
        return 1;
    else
        return 0;
}

//главная функция
int cli(int arg_count, char *num_arg[])
{
    if(arg_count < 2)
    {
        fprintf(stderr, "Ошибка: Отстутсвуют аргументы\n");
        return 2;
    }

    if(strcmp(num_arg[1], "--help") == 0)
    {
        if(arg_count > 2)
        {
            fprintf(stderr, "Ошибка: Много аргументов\n");
            return 2;
        }
        printf("Использование: ./csvstat <command> <file name>\n");
        printf("Команды:\n");
        printf("schema    Показать структуру таблицы и типы столбцов\n");
        printf("stats     Вывести подробную статистику по столбцам\n");
        return 0;
    }

    if(strcmp(num_arg[1], "schema") == 0 || strcmp(num_arg[1], "stats") == 0)
    {
        if(arg_count != 3)
        {
            fprintf(stderr, "Ошибка: Неверный номер для команды '%s'\n", num_arg[1]);
            return 2;
        }

        FILE *f = fopen(num_arg[2], "r");
        if (!f) 
        {
            fprintf(stderr, "Ошибка: Не удалось открыть фаил '%s'\n", num_arg[2]);
            return 1;
        }
        
        char buffer[1024];

        if (fgets(buffer, sizeof(buffer), f) == NULL)
        {
            fprintf(stderr, "Ошибка: Фаил пустой\n");
            fclose(f);
            return 1;
        }

        int cols = 1;
        for(int i = 0; buffer[i] != 0; i++)
        {
            if(buffer[i] == ',')
                cols++;
        }

        char *headers[1024];
        split_line(buffer, headers, cols);

        int rows_for_memory = 100; //начальная вместимость
        int rows = 0; 
        char ***data = malloc(rows_for_memory * sizeof(char **));
        if (data == NULL)
        {
            fprintf(stderr, "Ошибка: Не удалось выделить память\n");
            for(int i = 0; i < cols; i++)
                free(headers[i]);
            fclose(f);
            return 1;
        }

        while(fgets(buffer, sizeof(buffer), f))
        {
            delete_end(buffer);
            if(strlen(buffer) != 0)
            {
                if (rows >= rows_for_memory)
                {
                    rows_for_memory = rows_for_memory * 2;
                    char ***temp = realloc(data, rows_for_memory * sizeof(char **));
                    if (temp == NULL)
                    {
                        fprintf(stderr, "Ошибка: Не удалось выделить дополнительную память\n");
                        for(int i = 0; i < cols; i++)
                            free(headers[i]);
                        for(int i = 0; i < rows; i++) 
                        {
                            for(int k = 0; k < cols; k++) free(data[i][k]);
                            free(data[i]);
                        }
                        free(data);
                        fclose(f);
                        return 1;
                    }
                    data = temp;
                }

                // выделение памяти под колонки текущей строки
                data[rows] = malloc(cols * sizeof(char *));
                if (data[rows] == NULL)
                {
                    fprintf(stderr, "Ошибка: Не удалось выделить память для строки\n");
                    fclose(f);
                    return 1;
                }

                split_line(buffer, data[rows], cols);
                rows++;
            }
        }
        fclose(f);


        Column_type types[1024];
        for(int j = 0; j < cols; j++)
        {
            types[j] = type_int;
            for(int i = 0; i < rows; i++)
            {
                if(is_int(data[i][j]) == 0)
                {
                    if(is_double(data[i][j]) == 1)
                        types[j] = type_double;
                }
            }
        }

        if (strcmp(num_arg[1], "schema") == 0)
        {
            printf("rows %d\n", rows);
            printf("columns %d\n", cols);
            for(int i = 0; i < cols; i++)
            {
                const char *type_name;
                if (types[i] == type_double) {
                    type_name = "DOUBLE";
                } else {
                    type_name = "INT";
                }
                printf("%d \"%s\" %s\n", i, headers[i], type_name);
            }
        }

        else // stats
        {
            printf("rows: %d\n", rows);
            printf("cols: %d\n", cols);
            
            for(int i = 0; i < cols; i ++)
            {
                if(types[i] == type_double)
                    printf("type DOUBLE\n");
                else
                    printf("type INT\n");
                if(types[i] == type_int)
                {
                    double nulls = 0; // для ср ариф
                    long long min;
                    long long max;

                    double sum = 0;
                    double mean; // ср ариф

                    long long positive = 0;
                    long long negative = 0;
                    long long even = 0; // четность

                    int first_num = 1;
                    for(int j = 0; j < rows; j++)
                    {   
                        if(strlen(data[j][i]) == 0)
                            nulls++;

                        char *end_ptr;
                        errno = 0;
                        long long value = strtoll(data[j][i], &end_ptr, 10); // из строки в число
                        if (first_num == 1)
                        {
                            min = value;
                            max = value;
                            first_num = 0;
                        }
                        else
                        {
                            if (value < min)
                                min = value;
                            if (value > max)
                                max = value;
                        }

                        sum = sum + value;

                        if (value > 0)
                            positive++;
                        else
                            negative++;

                        if (value % 2 == 0)
                            even++;
                    }
                    mean = sum / ((double)rows - nulls);

                    printf("column %d\n", i);
                    printf("name %s\n", headers[i]); 
                    printf("type INT\n");
                    printf("nulls %d\n", (int)nulls);
                    printf("min %lld\n", min);
                    printf("max %lld\n", max);
                    printf("mean %f\n", mean);
                    printf("positive %lld\n", positive);
                    printf("negative %lld\n", negative);
                    printf("even %lld\n", even);
                    printf("\n");
                }

                else // double
                {
                    double nulls_dbl = 0; // для ср ариф
                    double min_dbl;
                    double max_dbl;

                    double sum_dbl = 0;
                    double mean_dbl; // ср ариф

                    long long positive_dbl = 0;
                    long long negative_dbl = 0;
                    long long integer_dbl = 0; // INT .0

                    int first_num_dbl = 1;

                    for(int k = 0; k < rows; k++)
                    {
                        if(strlen(data[k][i]) == 0)
                            nulls_dbl++;
                        double value_dbl = atof(data[k][i]); // преобразование из строки в double

                        if (first_num_dbl == 1)
                        {
                            min_dbl = value_dbl;
                            max_dbl = value_dbl;
                            first_num_dbl = 0;
                        }           
                        else
                        {
                            if (value_dbl < min_dbl)
                                min_dbl = value_dbl;
                            if (value_dbl > max_dbl)
                                max_dbl = value_dbl;
                            
                        }

                        sum_dbl = sum_dbl + value_dbl;

                        mean_dbl = sum_dbl / ((double)rows - nulls_dbl);

                        if (value_dbl > 0)
                            positive_dbl++;
                        else
                            negative_dbl++;

                        if (value_dbl - ceil(value_dbl) == 0.0)
                            integer_dbl++;
                    }

                    printf("column %d\n", i); // 
                    printf("name %s\n", headers[i]); 
                    printf("type DOUBLE\n");
                    printf("nulls %d\n", (int)nulls_dbl);
                    printf("min %f\n", min_dbl);
                    printf("max %f\n", max_dbl);
                    printf("mean %f\n", mean_dbl);
                    printf("positive %lld\n", positive_dbl);
                    printf("negative %lld\n", negative_dbl);
                    printf("integer %lld\n", integer_dbl);
                    printf("\n");
                }
            }
        }
    
        // очистка памяти
        for(int i = 0; i < cols; i++) 
        {
            free(headers[i]); 
        }

        for(int i = 0; i < rows; i++) 
        {
            for(int k = 0; k < cols; k++)
            {
                if (i < rows)
                {
                    free(data[i][k]);
                }
            }
            free(data[i]); 
        }
        free(data);

        return 0;
    }

    return 2;
}

