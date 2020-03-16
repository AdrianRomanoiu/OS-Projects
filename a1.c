#include <stdio.h>
#include <memory.h>
#include <malloc.h>
#include <sys/types.h>
#include <dirent.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>

#define TRUE 1
#define FALSE 0


typedef struct _SECTION_HEADER{

    char Name[11];
    int  Type;
    int  Offset;
    int  Size;
}SECTION_HEADER;


char *strrev(char *print);


int variant(int NrOfArguments)
{
    if (NrOfArguments != 2)
    {
        printf("ERROR\nToo many arguments for [variant] command");
        return -1;
    }

    printf("69712\n");

    return 0;
}



int list(const char* Path, int Execute_perm, size_t File_size, int Recursive, int FileSizeFlag)
{
    DIR* directory = NULL;
    struct dirent* entry = NULL;
    char* fullPath = NULL;
    struct stat statBuffer;

    directory = opendir(Path);
    if (directory == NULL)
    {
        printf("ERROR\nCould not open directory");
        return -1;
    }

    while ((entry = readdir(directory)) != NULL)
    {
        if (strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0)
        {
            size_t length = strlen(Path) + 1 + strlen(entry->d_name) + 1;
            fullPath = (char*)malloc(length * sizeof(char));
            snprintf(fullPath, length, "%s/%s", Path, entry->d_name);
            if (lstat(fullPath, &statBuffer) == 0)
            {
                if (S_ISDIR(statBuffer.st_mode))
                {
                    if (FileSizeFlag == FALSE || (Execute_perm == TRUE && (S_IXUSR & statBuffer.st_mode)))
                    {
                        printf("%s/%s\n", Path, entry->d_name);
                    }

                    if (Recursive == TRUE)
                    {
                        if (list(fullPath, Execute_perm, File_size, Recursive, FileSizeFlag))
                        {
                            return -1;
                        }
                    }
                }
                else if (S_ISREG(statBuffer.st_mode))
                {
                    int printedFlag = FALSE;

                    if (FileSizeFlag == TRUE  && statBuffer.st_size > File_size)
                    {
                        printf("%s\n", fullPath);
                        printedFlag = TRUE;
                    }

                    if (printedFlag == FALSE && (Execute_perm == TRUE && (S_IXUSR & statBuffer.st_mode)))
                    {
                        printf("%s\n", fullPath);
                        printedFlag = TRUE;
                    }

                    if (printedFlag == FALSE && FileSizeFlag == FALSE && Execute_perm == FALSE)
                    {
                        printf("%s\n", fullPath);
                    }

                }
            }

            free(fullPath);
        }
    }

    closedir(directory);

    return 0;
}

int parseListOptions(int NrOfArguments, const char* Arguments[])
{
    if (NrOfArguments > 6)
    {
        printf("ERROR\nToo many arguments for [list] command");
        return -1;
    }

    char* path = NULL;
    int recursive = FALSE;
    int executePerm = FALSE;
    int fileSizeFlag = FALSE;
    size_t fileSize = 0;

    for (int i = 2; i < NrOfArguments; i++)
    {
        if (recursive == FALSE && !strcmp(Arguments[i], "recursive"))
        {
            recursive = TRUE;
        }

        if (path == NULL && strstr(Arguments[i], "path=") != NULL)
        {
            size_t length = strlen(Arguments[i]) - 5 + 1;
            path = (char*)malloc(length * sizeof(char));
            if (path == NULL)
            {
                printf("ERROR\nFailed to allocate memory for path");
                return -1;
            }

            memcpy(path, Arguments[i] + 5, length);
        }

        if (executePerm == FALSE && !strcmp(Arguments[i], "has_perm_execute"))
        {
            executePerm = TRUE;
        }

        if (fileSizeFlag == FALSE && strstr(Arguments[i], "size_greater=") != NULL)
        {
            if (sscanf(Arguments[i] + 13, "%lu", &fileSize) == 0)
            {
                printf("ERROR\nsize_greater value is not an int");
                free(path);
                return -1;
            }
            fileSizeFlag = TRUE;
        }
    }

    if (path == NULL)
    {
        printf("ERROR\ninvalid directory path");
        return -1;
    }

    printf("SUCCESS\n");
    if (list(path, executePerm, fileSize, recursive, fileSizeFlag))
    {
        free(path);
        return -1;
    }

    free(path);

    return 0;
}



int parse(const char* Path)
{
    int version;
    int nrOfSections;
    int headerSize;

    unsigned char buffer[4];
    memset(buffer, 0, 4);

    int* converter;

    int fd = -1;
    fd = open(Path, O_RDONLY);
    if (fd == -1)
    {
        printf("ERROR\nCould not open the file");
        close(fd);
        return -1;
    }

    lseek(fd, -2, SEEK_END);
    read(fd, buffer, 2);
    if (buffer[0] != 't' || buffer[1] != 'Y')
    {
        printf("ERROR\nwrong magic");
        close(fd);
        return -1;
    }

    lseek(fd, -4, SEEK_END);
    read(fd, buffer, 2);
    converter = (int*)buffer;
    headerSize = (*converter);
    lseek(fd, -headerSize, SEEK_END);
    memset(buffer, 0, 2);
    //We are at the beggining of the header.

    read(fd, buffer, 1);
    converter = (int*)buffer;
    version = (*converter);
    if (version < 96 || version > 135)
    {
        printf("ERROR\nwrong version");
        close(fd);
        return -1;
    }

    read(fd, buffer, 1);
    converter = (int*)buffer;
    nrOfSections = (*converter);
    if (nrOfSections < 6 || nrOfSections > 18)
    {
        printf("ERROR\nwrong sect_nr");
        close(fd);
        return -1;
    }

    SECTION_HEADER ** sections = (SECTION_HEADER**)malloc(nrOfSections * sizeof(SECTION_HEADER*));
    for (int i = 0; i < nrOfSections; i++)
    {
        sections[i] = (SECTION_HEADER*)malloc(sizeof(SECTION_HEADER));

        read(fd, sections[i]->Name, 10);
        sections[i]->Name[10] = 0;

        read(fd, buffer, 4);
        converter = (int*)buffer;
        if ((*converter) != 49 && (*converter) != 12 && (*converter) != 60 && (*converter) != 78)
        {
            printf("ERROR\nwrong sect_types");

            for (int j = 0; j <= i; j++)
                free(sections[j]);
            free(sections);

            return -1;
        }

        sections[i]->Type = (*converter);

        read(fd, buffer, 4);
        converter = (int*)buffer;
        sections[i]->Offset = (*converter);

        read(fd, buffer, 4);
        converter = (int*)buffer;
        sections[i]->Size = (*converter);
    }

    printf("SUCCESS\nversion=%d\nnr_sections=%d\n", version, nrOfSections);
    for (int i = 0; i < nrOfSections; i++)
    {
        printf("section%d: %s %d %d\n", i + 1, sections[i]->Name, sections[i]->Type, sections[i]->Size);
    }

    for (int i = 0; i < nrOfSections; i++)
        free(sections[i]);
    free(sections);

    close(fd);

    return 0;
}

int parseParseOpotions(int NrOfArguments, const char* Arguments[])
{
    if (NrOfArguments > 3)
    {
        printf("ERROR\nToo many arguments for [parse] command");
        return -1;
    }

    char* path = NULL;
    if (strstr(Arguments[2], "path=") != NULL)
    {
        size_t length = strlen(Arguments[2]) - 5 + 1;
        path = (char*)malloc(length);
        if (path == NULL)
        {
            printf("ERROR\nCould not alocate memory for path");
            return -1;
        }

        memcpy(path, Arguments[2] + 5, length);
    }
    else
    {
        printf("ERROR\ninvalid directory path");
        return -1;
    }

    if (parse(path))
    {
        free(path);
        return -1;
    }

    free(path);
    return 0;
}


char *strrev(char* print)
{
    size_t length = strlen(print);
    char* reuslt = (char*)malloc(length + 1);

    for (int i = length - 1; i >= 0; i--)
    {
        reuslt[length - 1- i] = print[i];
    }

    reuslt[length] = 0;
    memcpy(print, reuslt, length + 1);

    free(reuslt);

    return  print;
}

int extract(const char* Path, int Section, int Line)
{
    int version;
    int nrOfSections;
    int headerSize;

    char* toPrint = NULL;

    unsigned char buffer[4];
    memset(buffer, 0, 4);

    int fd = -1;
    fd = open(Path, O_RDONLY);
    if (fd == -1)
    {
        printf("ERROR\nCould not open the file");
        return -1;
    }

    lseek(fd, -2, SEEK_END);
    read(fd, buffer, 2);
    if (buffer[0] != 't' || buffer[1] != 'Y')
    {
        printf("ERROR\ninvalid file");
        return -1;
    }

    lseek(fd, -4, SEEK_END);
    read(fd, buffer, 2);
    headerSize = (*((int*)buffer));
    lseek(fd, -headerSize, SEEK_END);
    memset(buffer, 0, 2);
    //We are at the beggining of the header.

    read(fd, buffer, 1);
    version = (*((int*)buffer));
    if (version < 96 || version > 135)
    {
        printf("ERROR\ninvalid file");
        return -1;
    }

    read(fd, buffer, 1);
    nrOfSections = (*((int*)buffer));
    if (nrOfSections < 6 || nrOfSections > 18)
    {
        printf("ERROR\ninvalid file");
        return -1;
    }
    if (Section > nrOfSections)
    {
        printf("ERROR\ninvalid section");
        return -1;
    }

    SECTION_HEADER ** sections = (SECTION_HEADER**)malloc(nrOfSections * sizeof(SECTION_HEADER*));
    for (int i = 0; i < nrOfSections; i++)
    {
        sections[i] = (SECTION_HEADER*)malloc(sizeof(SECTION_HEADER));

        read(fd, sections[i]->Name, 10);
        sections[i]->Name[10] = 0;

        read(fd, buffer, 4);
        int type = (*((int*)buffer));
        if (type != 49 && type != 12 && type != 60 && type != 78)
        {
            printf("ERROR\ninvalid file");

            for (int j = 0; j <= i; j++)
                free(sections[j]);
            free(sections);

            return -1;
        }

        sections[i]->Type = type;

        read(fd, buffer, 4);
        sections[i]->Offset = (*((int*)buffer));

        read(fd, buffer, 4);
        sections[i]->Size = (*((int*)buffer));
    }

    lseek(fd, sections[Section - 1]->Offset + sections[Section - 1]->Size, SEEK_SET);
    char sectionBuffer[2048];

    int cat = sections[Section - 1]->Size / 2048;
    int rest = sections[Section - 1]->Size % 2048;

    int lineStart = -1;
    int lineEnd = -1;

    if (cat != 0)
        lseek(fd, -2048, SEEK_CUR);

    for (int i = 0; i < cat && Line != 0; i++)
    {
        size_t length = read(fd, sectionBuffer, 2048);

        int j = length - 1;
        while (j >= 0)
        {
            if (sectionBuffer[j] == '\n')
            {
                Line--;

                if (Line == 1) {
                    lineEnd = lseek(fd, -(length - j), SEEK_CUR);
                    lseek(fd, length - j, SEEK_CUR);
                }
                else if (Line == 0)
                {
                    lineStart = lseek(fd, -(length - j), SEEK_CUR);

                    if (lineEnd == -1)
                    {
                        lineEnd = lseek(fd, sections[Section - 1]->Offset + sections[Section - 1]->Size, SEEK_SET);
                    }

                    break;
                }
            }

            j--;
        }

        lseek(fd, -2048 * 2, SEEK_CUR);
    }


    lseek(fd, -rest, SEEK_CUR);
    if (lineEnd == -1 || lineStart == -1)
    {
        read(fd, sectionBuffer, rest);
        int j = rest - 1;
        while (j >= 0)
        {
            if (sectionBuffer[j] == '\n')
            {
                Line--;

                if (Line == 1)
                {
                    if (lineEnd == -1)
                    {
                        lineEnd = lseek(fd, -(rest - j), SEEK_CUR);
                        lseek(fd, rest - j, SEEK_CUR);
                    }
                }
                else if (Line == 0)
                {
                    if (lineStart == -1)
                    {
                        lineStart = lseek(fd, -(rest - j), SEEK_CUR);

                        if (lineEnd == -1)
                        {
                            lineEnd = lseek(fd, rest, SEEK_CUR);
                        }

                        break;
                    }
                }
            }

            j--;
        }
    }

    size_t length = lineEnd - lineStart;
    toPrint = (char*)malloc(length * sizeof(char));
    lseek(fd, lineStart + 1, SEEK_SET);
    read(fd, toPrint, length);
    toPrint[length - 1] = '\0';

    printf("SUCCESS\n%s", strrev(toPrint));
    free(toPrint);

    for (int i = 0; i < nrOfSections; i++)
        free(sections[i]);
    free(sections);

    close(fd);

    return 0;
}

int parseExtractOptions(int NrOfArguments, const char* Arguments[])
{
    if (NrOfArguments > 5)
    {
        printf("ERROR\nToo many arguments for [extract] command");
        return -1;
    }

    char* path = NULL;
    int  section = -1;
    int  line = -1;

    for (int i = 2; i < NrOfArguments; i++)
    {
        if (strstr(Arguments[i], "path=") != NULL)
        {
            size_t length = strlen(Arguments[i]) - 5 + 1;
            path = (char*)malloc(length * sizeof(char));
            if (path == NULL)
            {
                printf("ERROR\nCould not alocate memory for path");
                return -1;
            }

            memcpy(path, Arguments[i] + 5, length);
        }
        else if (strstr(Arguments[i], "section=") != NULL)
        {
            if (sscanf(Arguments[i] + 8, "%d", &section) != 1)
            {
                printf("ERROR\ninvalid section");
                return -1;
            }
        }
        else if (strstr(Arguments[i], "line=") != NULL)
        {
            if (sscanf(Arguments[i] + 5, "%d", &line) != 1)
            {
                printf("ERROR\ninvalid line");
                return -1;
            }
        }
    }

    if (path == NULL)
    {
        printf("ERROR\ninvalid path");
        return -1;
    }

    if (extract(path, section, line))
    {
        free(path);
        return -1;
    }

    free(path);

    return 0;
}




int validateSF(const char* Path)
{
    int version;
    int nrOfSections;
    int headerSize;

    unsigned char buffer[4];
    memset(buffer, 0, 4);

    int fd = -1;
    fd = open(Path, O_RDONLY);
    if (fd == -1)
    {
        printf("ERROR\nCould not open the file");
        return -1;
    }

    lseek(fd, -2, SEEK_END);
    read(fd, buffer, 2);
    if (buffer[0] != 't' || buffer[1] != 'Y')
    {
        return -1;
    }

    lseek(fd, -4, SEEK_END);
    read(fd, buffer, 2);
    headerSize = (*((int*)buffer));
    lseek(fd, -headerSize, SEEK_END);
    memset(buffer, 0, 2);
    //We are at the beggining of the header.

    read(fd, buffer, 1);
    version = (*((int*)buffer));
    if (version < 96 || version > 135)
    {
        return -1;
    }

    read(fd, buffer, 1);
    nrOfSections = (*((int*)buffer));
    if (nrOfSections < 6 || nrOfSections > 18)
    {
        return -1;
    }

    SECTION_HEADER ** sections = (SECTION_HEADER**)malloc(nrOfSections * sizeof(SECTION_HEADER*));
    for (int i = 0; i < nrOfSections; i++)
    {
        sections[i] = (SECTION_HEADER*)malloc(sizeof(SECTION_HEADER));

        read(fd, sections[i]->Name, 10);
        sections[i]->Name[10] = 0;

        read(fd, buffer, 4);
        int type = (*((int*)buffer));
        if (type != 49 && type != 12 && type != 60 && type != 78)
        {

            for (int j = 0; j <= i; j++)
                free(sections[j]);
            free(sections);

            return -1;
        }

        sections[i]->Type = type;

        read(fd, buffer, 4);
        sections[i]->Offset = (*((int*)buffer));

        read(fd, buffer, 4);
        sections[i]->Size = (*((int*)buffer));

        if (sections[i]->Size > 1486)
        {
            for (int j = 0; j <= i; j++)
                free(sections[j]);
            free(sections);

            return -1;
        }
    }


    for (int i = 0; i < nrOfSections; i++)
        free(sections[i]);
    free(sections);

    close(fd);

    return 0;
}

int findAll(const char* Path)
{
    DIR* directory = NULL;
    struct dirent* entry = NULL;
    char* fullPath = NULL;
    struct stat statBuffer;

    directory = opendir(Path);
    if (directory == NULL)
    {
        printf("ERROR\nCould not open directory");
        return -1;
    }

    while ((entry = readdir(directory)) != NULL)
    {
        if (strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0)
        {
            size_t length = strlen(Path) + 1 + strlen(entry->d_name) + 1;
            fullPath = (char*)malloc(length * sizeof(char));
            snprintf(fullPath, length, "%s/%s", Path, entry->d_name);
            if (lstat(fullPath, &statBuffer) == 0)
            {
                if (S_ISDIR(statBuffer.st_mode))
                {
                    if (findAll(fullPath))
                    {
                        return -1;
                    }
                }
                else if (S_ISREG(statBuffer.st_mode))
                {
                    if (validateSF(fullPath) == 0)
                    {
                        printf("%s\n", fullPath);
                    }
                }
            }

            free(fullPath);
        }
    }

    closedir(directory);

    return 0;
}

int parseFindAllOptions(int NrOfArguments, const char* Arguments[])
{
    if (NrOfArguments > 3)
    {
        printf("ERROR\nToo many arguments for [parse] command");
        return -1;
    }

    char* path = NULL;
    if (strstr(Arguments[2], "path=") != NULL)
    {
        size_t length = strlen(Arguments[2]) - 5 + 1;
        path = (char*)malloc(length);
        if (path == NULL)
        {
            printf("ERROR\nCould not alocate memory for path");
            return -1;
        }

        memcpy(path, Arguments[2] + 5, length);
    }
    else
    {
        printf("ERROR\ninvalid directory path");
        return -1;
    }

    printf("SUCCESS\n");
    if (findAll(path))
    {
        return -1;
    }



    free(path);
    return 0;
}



int main(int argc, const char* argv[])
{
    if (argc == 1)
    {
        perror("Program runned with no arguments\n");
        return -1;
    }

    int nrOfArguments = argc;
    const char* option = argv[1];

    if (!strcmp(option, "variant"))
    {
        if (variant(nrOfArguments))
        {
            return -1;
        }
    }
    else if(!strcmp(option, "list"))
    {
        if (parseListOptions(nrOfArguments, argv))
        {
            return -1;
        }
    }
    else if (!strcmp(option, "parse"))
    {
        if (parseParseOpotions(nrOfArguments, argv))
        {
            return -1;
        }
    }
    else if (!strcmp(option, "extract"))
    {
        if (parseExtractOptions(nrOfArguments, argv))
        {
            return -1;
        }
    }
    else if (!strcmp(option, "findall"))
    {
        if (parseFindAllOptions(nrOfArguments, argv))
        {
            return -1;
        }
    }
    else
    {
        printf("ERROR\ninvalid command");
        return -1;
    }

    return 0;
}
