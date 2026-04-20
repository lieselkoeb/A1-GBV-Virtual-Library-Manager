#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "gbv.h"
#include "util.h"

/* AUXILIARY PROTOTYPES */
Document * createDocument ();

// Returns pointer to document
// Returns NULL if document is not found or invalid pointer
Document * findDocument(const Library *lib, const char *docname);

// Inserts metadata in lib->docs array, deleting previous metadata in the file
int updateMetadata (const Library *lib, const char *fileName);

/* AUXILIARY FUNCTIONS */
Document * createDocument () {
    Document *d;
    
    if (!(d = calloc(1, sizeof(Document)))) {
        perror("Calloc fail");
        return NULL;
    }
    
    return d;
}

Document * findDocument(const Library *lib, const char *docname) {
    Document *doc;
    int equal, i;
    
    if ((!lib) || (!docname)) {
        return NULL;
    }
    
    if (lib->count > 0) {
        equal = -1; // Initialize variable
        
        for (i = 0; i < lib->count; i++) {
            doc = &lib->docs[i];
            equal = strcmp(doc->name, docname);
            if (equal == 0) {
                break;
            }
        }
        
        if ((i == (lib->count - 1)) && (equal != 0)) { // Checks if the last document is not the 'docname'
            return NULL;
        }
    }
    else {
        return NULL;
    }
    
    return doc;
}

int updateMetadata (const Library *lib, const char *fileName) {
    FILE *f, *temp;
    Document *doc;
    size_t readsize, written;
    long stopReadingAt, currentOffset;
    int charsLeft, i;
    char *buffer;

    if ((!lib) || (!fileName)) {
        printf("Error: Invalid parameters on gbv_add\n");
        return 1;
    }

    f = fopen(fileName, "r");
    if (!f) {
        perror("Unable to read file 'f'");
        return 1;
    }    
    
    temp = fopen("temp.gbv", "w");
    if (!temp) {
        perror("Unable to write file 'w'");
        fclose(f);
        return 1;
    }

    // CREATE BUFFER
    buffer = calloc(1, BUFFER_SIZE); // Creates buffer
    if (!buffer) {
        perror("Fail to allocate buffer memory");
        fclose(f);
        fclose(temp);
        return 1;
    }

    fseek(f, sizeof(int), SEEK_SET); // Skips the number of documents
    fread(&stopReadingAt, sizeof(long), 1, f); // Stores the offset to Directory Area

    // WRITE NEW NUMBER OF DOCUMENTS AND DIRECTORY AREA OFFSET
    fwrite(&lib->count, sizeof(int), 1, temp);
    fwrite(&stopReadingAt, sizeof(long), 1, temp);
    
    // COPIES THE DOCUMENT AREA OF 'f' TO 'temp' 
        currentOffset = ftell(f);
        while ((currentOffset + BUFFER_SIZE) < stopReadingAt) {
            readsize = fread(buffer, 1, BUFFER_SIZE, f);
            written = fwrite(buffer, 1, BUFFER_SIZE, temp);
            if (readsize != written) {
                perror("Fail to write in 'temp.gbv'");
                fclose(f);
                fclose(temp);
                remove("temp.gbv");
                return 1;
            }

            currentOffset = ftell(f);
        }
        charsLeft = stopReadingAt - currentOffset;

        readsize = fread(buffer, 1, charsLeft, f);
        written = fwrite(buffer, 1, charsLeft, temp);

        if (readsize != written) {
            perror("Fail to write in 'temp.gbv'");
            fclose(f);
            fclose(temp);
            remove("temp.gbv");
            return 1;
        }
    
    // WRITE METADATA (DOCUMENTS) IN 'temp'
    for (i = 0; i < lib->count; i++) {
        doc = &lib->docs[i];
        fwrite(doc->name, (strlen(doc->name) + 1), 1, temp);
        fwrite(&doc->size, sizeof(long), 1, temp);
        fwrite(&doc->date, sizeof(time_t), 1, temp);
        fwrite(&doc->offset, sizeof(long), 1, temp);
    }

    fclose(temp);
    fclose(f);
    remove(fileName);
    rename("temp.gbv", fileName);

    return 0;
}

/* FUNCTIONS */

// Returns 0 on success
// Returns 1 on error
int gbv_open(Library *lib, const char *filename) {
    FILE *f;
    long offset;
    int i, j;
    char name[MAX_NAME];
    Document *doc;

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

        // CREATE THE lib->docs ARRAY
        lib->docs = calloc(1, (sizeof(Document) * lib->count));


        fread(&offset, sizeof(long), 1, f); // Stores the offset to Directory Area
        fseek(f, offset, SEEK_SET); // Jumps to Directory Area

        // INSERT EMPTY DOCUMENTS IN lib->docs
        for (j = 0; j < lib->count; j++) { // Iterating over array of documents
            doc = &lib->docs[j];
            for (i = 0; i < MAX_NAME; i++) { // Copying the document name
                fread(&name[i], sizeof(char), 1, f);
                if (name[i] == '\0') { // End of array
                    break;
                }
            }
            if (name[i] != '\0') { // Checking if the array was looped to the end
                printf("Error: Name of the file is too long\n");
                return 1;
            }
            strcpy(doc->name, name);
            fread(&doc->size, sizeof(long), 1, f);
            fread(&doc->date, sizeof(time_t), 1, f);
            fread(&doc->offset, sizeof(long), 1, f);
        }
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
    Document *doc, *doc2;
    void *buffer;
    long offset, docSize;
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
    fread(&offset, sizeof(long), 1, f); // Stores the offset to Directory Area
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
    }
    else {

        doc2 = realloc(lib->docs, (sizeof(Document) * (lib->count + 1)));
        if (!doc2) {
            perror("Fail to reallocate docs Array");
            // FUTURE: DEAL WITH THE FACT THAT IT HAS WRITTEN THE 'g' CONTENT INTO 'f' AND LOST 'f' METADATA
            fclose(f);
            fclose(g);
            return 1;
        }
        lib->docs = doc2;
        doc = &lib->docs[lib->count];

    }

    // INSERT DOCUMENT DATA
    strcpy(doc->name, docname);
    doc->offset = offset;
    time(&doc->date);
    doc->size = docSize;

    lib->count++;

    // WRITE NEW NUMBER OF DOCUMENTS AND OFFSET
    offset += docSize;
    fseek(f, 0, SEEK_SET);
    fwrite(&lib->count, sizeof(int), 1, f);
    fwrite(&offset, sizeof(long), 1, f);
    
    // CLOSE FILES AND FREE MEMORY
    free(buffer);
    fclose(f);
    fclose(g);

    // WRITE METADATA (DOCUMENTS) IN 'f'
    updateMetadata(lib, archive);

    return 0;
}

int gbv_list(const Library *lib) {
    int i;
    Document *doc;
    char buffer[20], input;

    if(!lib) {
        printf("invalid pointer in gbv_list\n");
        return 1;
    }

    if (lib->count > 0) { // If there are documents
        i = -1;            // Starting point
        input = 'n';    // Starting point
        while (input != 'q') {

            if (input == 'n') { // Prints the next document, or current document if it's the last document
                if (i < (lib->count - 1)) { // Checks if is not at the last document
                    i++;
                }
                else {
                    printf("WARNING: This is the last document\n");
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
                    printf("WARNING: This is the first document\n");
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
    else { // No documents in the file
        printf("There are no documents to show\n");
        return 1;
    }

    return 0;
}

int gbv_view(const Library *lib, const char *archive, const char *docname) {
    FILE *f;
    Document *doc;
    char *buffer;
    long bytesLeft, bytesToRead;
    char input;

    if ((!lib) || (!archive) || (!docname)) {
        printf("Error: Invalid parameters on gbv_view\n");
        return 1;
    }

    if ((strlen(docname) + 1) > MAX_NAME) {
        printf("Error: Document name length is too long\n");
        return 1;
    }

    if (lib->count > 0) {

        doc = findDocument(lib, docname);
        if (!doc) {
            printf("Document not found\n");
            return 1;
        }
        
        // ARCHIVE
        f = fopen(archive, "r+"); // Attempts to open existing file
        if (!f) {
            perror("Unable to read file 'f'");
            return 1;
        }

        // CREATE BUFFER
        buffer = calloc(1, BUFFER_SIZE); // Creates buffer
        if (!buffer) {
            perror("Fail to allocate buffer memory");
            fclose(f);
            return 1;
        }

        fseek(f, doc->offset, SEEK_SET); // Cursor at the beginning of the document

        bytesLeft = doc->size;
        bytesToRead = BUFFER_SIZE;
        input = 'n';
        while (input != 'q') {
            if (input == 'n') {
                if (bytesLeft != 0) {
                    if (bytesToRead > bytesLeft) {
                        bytesToRead = bytesLeft;
                    }
                    fread(buffer, 1, bytesToRead, f);
                    fwrite(buffer, 1, bytesToRead, stdout);
                    bytesLeft -= bytesToRead;
                }
                else {
                    printf("You've reached the end of the document");
                }
            }
            else if (input == 'p') {
                
                // Moves cursor to the beginning of the section
                if (bytesLeft == 0) {
                    fseek(f, -bytesToRead, SEEK_CUR);
                    bytesLeft += bytesToRead;
                }
                else if (bytesLeft < doc->size) {
                    fseek(f, -BUFFER_SIZE, SEEK_CUR);
                    bytesLeft += BUFFER_SIZE;
                }
                // Moves cursor to the beginning of previous section
                if (bytesLeft != doc->size) {
                    bytesToRead = BUFFER_SIZE;
                    fseek(f, -BUFFER_SIZE, SEEK_CUR);
                    fread(buffer, 1, bytesToRead, f);
                    fwrite(buffer, 1, bytesToRead, stdout);
                }
                else {
                    printf("You've reached the beginning of the document");
                }
            }

            printf("\n<< previous (p) | quit (q) | next (n) >>\n");
            scanf(" %c", &input);
        }
    }
    else { // No documents in the file
        printf("There are no documents to show\n");
        return 1;
    }

    free(buffer);
    fclose(f);

    return 0;
}

int gbv_remove(Library *lib, const char *archive, const char *docname) {

    if ((!lib) || (!archive) || (!docname)) {
        printf("Error: Invalid parameters on gbv_remove\n");
        return 1;
    }

    if ((strlen(docname) + 1) > MAX_NAME) {
        printf("Error: Document name length is too long\n");
        return 1;
    }

    return 0;
}