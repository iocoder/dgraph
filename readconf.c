/***************************/
/* READ CONFIGURATION FILE */
/***************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "proto.h"

#define CONF_FILE   "/etc/dgraph/system.properties"
#define MAX_BUF     4096

struct property {
    struct property *next;
    char *name;
    char *val;
} *properties = NULL;

void read_conf() {
    FILE *f;
    char line[MAX_BUF];
    char *tok, *name, *val;
    struct property *property;
    /* open configuration file */
    f = fopen(CONF_FILE, "r");
    /* read configuration line by line */
    while (fgets(line, MAX_BUF, f)) {
        /* split file by = */
        tok = strtok(line, " =\n");
        /* first token is property */
        name = tok;
        /* get next token */
        tok = strtok(NULL, " =\n");
        /* second token is val */
        val = tok;
        /* now evaluate properties */
#if 0
        printf("key: `%s', val: `%s'\n", name, val);
#endif
        /* allocate struct property */
        property = malloc(sizeof(struct property));
        /* copy strings to memory and initialize property struct */
        property->next = properties;
        property->name = strcpy(malloc(strlen(name)+1), name);
        property->val  = strcpy(malloc(strlen(val )+1), val );
        properties = property;
    }
    /* close file */
    fclose(f);
}

char *get_val_str(char *name, int index) {
    char fullname[MAX_BUF];
    struct property *prop = properties;
    /* copy name into fullname array */
    strcpy(fullname, name);
    /* if index is provided, convert it to string and concat. */
    if (index >= 0)
        sprintf(fullname+strlen(fullname), "%d", index);
    /* loop over properties */
    while (prop) {
        /* property name matching? */
        if (!strcmp(prop->name, fullname))
            return prop->val;
        prop = prop->next;
    }
    /* didn't find property */
    return NULL;
}

int get_val_int(char *name, int index) {
    /* get property value as string */
    char *ret = get_val_str(name, index);
    /* not found? */
    if (!ret)
        return -1;
    /* convert to integer */
    return atoi(ret);
}
