#include <stdbool.h>
#include <string.h>

bool compare_string_arrays(char *const *array1, char *const *array2)
{
    int i;

        for (i = 0; array1[i] != NULL; i++) {
        /* the second array is shorter or items are not equal */
        if (array2[i] == NULL || strcmp(array1[i], array2[i])) {
            return false;
        }
    }

    /* the first array is shorter */
    if (array2[i] != NULL) {
        return false;
    }

    return true;
}
