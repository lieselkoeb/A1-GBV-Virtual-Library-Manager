#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "gbv.h"
#include "util.h"

/* AUXILIARY PROTOTYPES */
Document * createDocument ();

// Searches for a document by name in the library directory.
// Returns the index of the matching document.
// Returns -1 if the document does not exist.
int findDocument(const Library *lib, const char *docname);

// Writes all metadata from lib->docs to the end of the given library file.
// Always appends the directory area at the end of the file and does not check for existing metadata entries.
// Returns 0 on success.
// Returns 1 on failure.
int updateMetadata (Library *lib, const char *docname);

// Inserts a new document into the library container file.
// Writes only the raw document data and updates the in-memory directory,
// but does not write metadata to disk.
// Assumes the document does not already exist in the library.
// Returns 0 on success.
// Returns 1 on failure, leaving the file unchanged.
int insertNewDocument(Library *lib, const char *archive, const char *docname);

// Replaces an existing document in the library while preserving its original offset
// whenever possible, adjusting following documents if necessary.
// Updates the in-memory directory but does not rewrite metadata.
// Returns 0 on success.
// Returns 1 on failure.
int replaceDocument (Document *docs, int docIndex, const char *archive, const char *docname);

// Creates a temporary backup copy of the given file by appending ".temp"
// to its original name.
// Example: "file.txt" becomes "file.txt.temp".
// Returns the name of the temporary file on success.
// Returns NULL on failure.
char * copyFile(const char *file);


/* AUXILIARY FUNCTIONS */
Document * createDocument () {
    Document *d;
    
    if (!(d = calloc(1, sizeof(Document)))) {
        perror("Calloc fail");
        return NULL;
    }
    
    return d;
}

int findDocument(const Library *lib, const char *docname) {
    Document *doc;
    int equal, i;
    
    { // TEST IF PARAMETERS ARE VALID
        if ((!lib) || (!docname)) {
            printf("Error: invalid parameters\n");
            return -2;
        }
        if ((strlen(docname) + 1) > MAX_NAME) {
            printf("Error: Document name length is too long\n");
            return -2;
        }
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
            return -1;
        }
    }
    else {
        return -1;
    }
    
    return i;
}

char * copyFile(const char *fileName) {
    FILE *f, *t;
    size_t bytesRead, bytesWritten;
    char buffer[BUFFER_SIZE], *temp;

    { // TEST IF PARAMETER IS VALID
        if (!fileName) {
            return NULL;
        }
    }

    { // OPEN GIVEN FILE
        f = fopen(fileName, "rb");
        if (!f) {
            perror("copyFile - Unable to open f");
            return NULL;
        }
    }

    { // COPIES fileName to temp and ADDS ".temp" AT THE END
        temp = calloc(1, MAX_NAME + 6);
        if (!temp) {
            perror("copyFile - Unable to alloc memory to temp");
            fclose(f);
            return NULL;
        }
        strcpy(temp, fileName);
        strcat(temp, ".temp");
    }
    
    { // CREATES TEMPORARY FILE
        t = fopen(temp, "wb");
        if (!t) {
            perror("copyFile - Unable to open t");
            free(temp);
            fclose(f);
            return NULL;
        }
    }

    { // COPIES GIVEN FILE TO .temp FILE
        while ((bytesRead = (fread(buffer, 1, BUFFER_SIZE, f))) > 0) {
            bytesWritten = fwrite(buffer, 1, bytesRead, t);
            if (bytesRead != bytesWritten) {
                printf("copyFile: Fail to write in temp\n");
                fclose(t);
                fclose(f);
                remove(temp);
                free(temp);
                return NULL;
            }
        }
    }
    
    fclose(f);
    fclose(t);
    return temp;
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
        // CREATE THE lib->docs ARRAY
        lib->docs = calloc(1, (sizeof(Document) * lib->count));


        fread(&offset, sizeof(long), 1, f); // Stores the offset to Directory Area
        fseek(f, offset, SEEK_SET); // Jumps to Directory Area

        // INSERT DOCUMENTS IN lib->docs
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
        fclose(f);
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
    char *temp;
    int docIndex, ret;

    { // TEST IF PARAMETERS ARE VALID
        if ((!lib) || (!archive) || (!docname)) {
            printf("Error: Invalid parameters on gbv_add\n");
            return 1;
        }
        if ((strlen(docname) + 1) > MAX_NAME) {
            printf("Error: Document name length is too long\n");
            return 1;
        }
    }

    { // COPIES THE LIBRARY FILE TO RETURN TO PREVIOUS STATUS IN CASE OF ERROR
        temp = copyFile(docname);
        if (!temp) {
            printf("Error in copying file\n");
            return 1;
        }
    }
    
    { // EITHER ADDS A NEW DOCUMENT OR REPLACES AN OLD DOCUMENT
        if ((docIndex = findDocument(lib, docname)) >= 0) {
            ret = replaceDocument (lib->docs, docIndex, archive, docname);
            if (ret == 1) {
                printf("Unable to substitute document\n");
                free(temp);
                remove(docname);
                rename(temp, docname);
                return 1;
            }
        }
        else if (docIndex == -1) {
            ret = insertNewDocument(lib, archive, docname);
            if (ret == 1) {
                printf("Unable to insert new document\n");
                free(temp);
                remove(docname);
                rename(temp, docname);
                return 1;
            }
        }
        else {
            printf("gbv_add - Fail to search file in lib->docs\n");
            free(temp);
            remove(docname);
            rename(temp, docname);
            return 1;
        }
    }
    
    { // UPDATES METADATA
        ret = updateMetadata(lib, docname);
        if (ret == 1) {
            printf("Unable to update metadata\n");
            free(temp);
            remove(docname);
            rename(temp, docname);
            return 1;
        }
    }
    
    remove(temp);
    free(temp);
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