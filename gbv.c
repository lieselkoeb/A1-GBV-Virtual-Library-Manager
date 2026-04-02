#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "gbv.h"
#include "util.h"

/* AUXILIARY PROTOTYPES */
Document * createDocument ();

/* AUXILIARY FUNCTIONS */
Document * createDocument () {
    Document *d;

    if (!(d = calloc(1, sizeof(Document)))) {
        perror("Calloc fail");
        return NULL;
    }

    return d;
}

/* FUNCTIONS */

// Returns 0 on success
// Returns 1 on error
int gbv_open(Library *lib, const char *filename) {
    FILE *f;

    f = fopen(filename, "r+"); // Attempts to open existing file

    if (!f) { // Files does not exists
        if (gbv_create(filename)) { // Creates new file
            return 1;
        }
        if (!(lib = calloc(1, sizeof(Library)))) { // Creates new empty Library
            return 1;
        }
        return 0;
    }

    fread (&lib->count, sizeof(int), 1, f);
    if (lib->count == 0) { // If there are no documents in the library
        lib->docs = NULL;
    }
    else {
        // FUTURE: POPULATE STRUCT DOCUMENTS
    }

    fclose(f);
    return 0;
}

// Returns 0 on success
// Returns 1 on error
int gbv_create(const char *filename) {
    FILE *f;
    int ret, i = 0;
    long j = sizeof(int) + sizeof(long);

    f = fopen (filename, "w"); // Creates the document
    if (!f) {
        return 1;
    }

    ret = fwrite(&i, sizeof(int), 1, f); // Inserts number of documents
    if (!ret) {
        return 1;
    }
    ret = fwrite(&j, sizeof(long), 1, f); // Inserts offset to Directory Area
    if (!ret) {
        return 1;
    }

    fclose(f);

    return 0;
}

// Returns 0 on success
// Returns 1 on error
int gbv_add(Library *lib, const char *archive, const char *docname) {
    FILE *f, *g;
    Document *doc, *iDoc;
    void *buffer;
    long offset, docSize;
    int i;
    size_t readsize, written;

    if ((!lib) || (!archive) || (!docname)) {
        printf("Error: Invalid parameters on gbv_add\n");
        return 1;
    }

    if ((strlen(docname) + 1) > MAX_NAME) {
        printf("Error: Document name length is too long\n");
        return 1;
    }
    
    // ARCHIVE
    f = fopen(archive, "r+"); // Attempts to open existing file
    if (!f) {
        perror("Unable to read file 'f'");
        return 1;
    }
    
    fseek(f, sizeof(int), SEEK_SET); // Skips the number of documents
    fread(&offset, sizeof(long), 1, f); // Stores the offset to Drectory Area
    fseek(f, offset, SEEK_SET); // Jumps to Directory Area
    
    // DOCUMENT
    g = fopen(docname, "r"); // Attempts to open existing file
    if (!g) {
        perror("Unable to read file 'g'");
        fclose(f);
        return 1;
    }
    
    // CREATE BUFFER
    buffer = calloc(1, BUFFER_SIZE); // Creates buffer
    if (!buffer) {
        perror("Fail to allocate buffer memory");
        fclose(f);
        fclose(g);
        return 1;
    }

    docSize = 0;
    while ((readsize = fread(buffer, 1, BUFFER_SIZE, g)) > 0) {
        written = fwrite(buffer, 1, readsize, f);
        if (written != readsize) {
            perror("Fail to write in 'f'");
            // FUTURE: DEAL WITH THE FACT THAT IT HAS POSSIBLY WRITTEN THE 'g' CONTENT INTO 'f' AND LOST 'f' METADATA
            free(buffer);
            fclose(f);
            fclose(g);
            return 1;
        }
        docSize += (long) readsize;
    }
    
    if (ferror(g)) {
        perror("Fail to read 'g' completely");
        // FUTURE: DEAL WITH THE FACT THAT IT HAS POSSIBLY WRITTEN THE 'g' CONTENT INTO 'f' AND LOST 'f' METADATA
        free(buffer);
        fclose(f);
        fclose(g);
        return 1;
    }

    // INSERT NEW DOCUMENT IN LIBRARY DOCUMENTS ARRAY
    if (lib->count == 0) { // First document in the library
        lib->docs = calloc(1, sizeof(Document));
        if (!lib->docs) {
            perror("Fail to allocate docs Array");
            // FUTURE: DEAL WITH THE FACT THAT IT HAS WRITTEN THE 'g' CONTENT INTO 'f' AND LOST 'f' METADATA
            fclose(f);
            fclose(g);
            return 1;
        }
        doc = &lib->docs[0];
        
        // INSERT DOCUMENT DATA
        strcpy(doc->name, docname);
        doc->offset = offset;
        time(&doc->date);
        doc->size = docSize;

        lib->count++;
    }
    else {

        /* FUTURE:
        - REALLOC lib->docs to 1 size more
        - INSERT 'doc' in array lib->docs
        - 
        -
        -
        */
    }

    // WRITE NEW NUMBER OF DOCUMENTS AND OFFSET
    offset += docSize;
    fseek(f, 0, SEEK_SET);
    fwrite(&lib->count, sizeof(int), 1, f);
    fwrite(&offset, sizeof(long), 1, f);
    
    // WRITE METADATA (DOCUMENTS) IN 'f'
    fseek(f, 0, SEEK_END);

    for (i = 0; i < lib->count; i++) {
        iDoc = &lib->docs[i];
        fwrite(iDoc->name, (strlen(iDoc->name) + 1), 1, f);
        fwrite(&iDoc->size, sizeof(long), 1, f);
        fwrite(&doc->date, sizeof(time_t), 1, f);
        fwrite(&iDoc->offset, sizeof(long), 1, f);
    }

    free(buffer);
    fclose(f);
    fclose(g);
    return 0;
}

int gbv_view(const Library *lib, const char *docname) {
    int i, equal;
    Document *doc;
    char buffer[20], input;

    if((!lib) || ((!docname))) {
        printf("invalid pointer in gbv_view\n");
        return 1;
    }

    if (lib->count > 0) { // If there are documents
        for (i = 0; i < lib->count; i++) { // Look for document
            doc = &lib->docs[i];
            equal = strcmp(doc->name, docname);
            if (equal == 0) { // Found the document
                break;
            }
        }

        if ((equal != 0) && (i == (lib->count -1))) { // Check if looped through all documents but didn't find it
            printf("document not found\n");
            return 1;
        }
        else { // Document was found
            i--;            // Starting point
            input = 'n';    // Starting point
            while (input != 'q') {

                if (input == 'n') { // Prints the next document, or current document if it's the last document
                    if (i < (lib->count - 1)) { // Checks if is not at the last document
                        i++;
                    }
                    else {
                        printf("This is the last document\n");
                    }
                    doc = &lib->docs[i];
                    printf("| DOCUMENT: %d/%d\n", (i + 1), lib->count);
                    printf("-------------------------\n");
                    printf("| NAME:   %s\n", doc->name);
                    printf("| SIZE:   %ld\n", doc->size);
                    format_date(doc->date, buffer, sizeof(buffer));
                    printf("| DATE:   %s\n", buffer);
                    printf("| OFFSET: %ld\n", doc->offset);
                    printf("-------------------------\n");
                    
                }
                else if (input == 'p') { // Prints the previous document, or current document if it's the first document
                    if (i > 0) {
                        i --;
                    }
                    else {
                        printf("This is the first document\n");
                    }
                    doc = &lib->docs[i];
                    printf("| DOCUMENT: %d/%d\n", (i + 1), lib->count);
                    printf("-------------------------\n");
                    printf("| NAME:   %s\n", doc->name);
                    printf("| SIZE:   %ld\n", doc->size);
                    format_date(doc->date, buffer, sizeof(buffer));
                    printf("| DATE:   %s\n", buffer);
                    printf("| OFFSET: %ld\n", doc->offset);
                    printf("-------------------------\n");
                }
                
                printf("|-----MENU-----|\n");
                printf("| n: next      |\n");
                printf("| p: previous  |\n");
                printf("| q: quit      |\n");
                printf("|--------------|\n");
                printf("Select an option: ");
                scanf(" %c", &input);
                printf("\n");
            }
        }
    }
    else { // No documents in the file
        printf("There are no documents to show\n");
        return 1;
    }

    return 0;
}