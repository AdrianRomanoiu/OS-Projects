#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <sys/shm.h>
#include <sys/mman.h>



#define WRITE_FIFO_NAME "RESP_PIPE_69712"
#define READ_FIFO_NAME "REQ_PIPE_69712"

#define VARIANT 69712

#define SHARED_MEMORY_KEY 18209

#define ALIGNMENT 5120


int gFdWrite = -1;
int gFdRead  = -1;

void* gMappedFile = NULL;
void* gSharedZone = NULL;



void WriteString(char string[])
{
    int stringLength = strlen(string);

    if (write(gFdWrite, &stringLength, 1) == -1)
    {
        perror("ERROR\nfailed to write string size to the pipe");
        exit(1);
    }

    if (write(gFdWrite, string, stringLength) == -1)
    {
        perror("ERROR\nfailed to write string to the pipe");
        exit(1);
    }
}

void WriteNumber(unsigned int number)
{
    if (write(gFdWrite, &number, sizeof(unsigned int)) == -1)
    {
        perror("ERROR\nfailed to write number to the pipe");
        exit(1);
    }
}

char* ReadString()
{
    char* toReturn = NULL;

    char lengthBuffer[4];
    memset(lengthBuffer, 0, 4);

    int length;

    if (read(gFdRead, &lengthBuffer, 1) == -1)
    {
        perror("ERROR\nfailed read string length from pipe");
        exit(1);
    }

    length = *((int*)lengthBuffer);
    toReturn = (char*) malloc((length + 1) * sizeof(char));

    if (read(gFdRead, toReturn, length) == -1)
    {
        perror("ERROR\nfailed to read string from pipe");
        exit(1);
    }
    toReturn[length] = '\0';

    return toReturn;
}

unsigned int ReadNumber()
{
    unsigned int toReturn = 0;

    if (read(gFdRead, &toReturn, sizeof(unsigned int)) == -1)
    {
        perror("ERROR\nfailed to read number from the pipe");
        exit(1);
    }

    return toReturn;
}

int GetNumberFromMappedFile(int firstByteIndex, int noBytes)
{
    int toReturn;

    char buffer[4];
    memset(buffer, 0, 4);

    for (int i = 0; i < noBytes; i++)
    {
        buffer[i] = ((char*)gMappedFile)[firstByteIndex + i];
    }
    toReturn = *((int*)buffer);

    return toReturn;
}



void Connect()
{
    unlink(WRITE_FIFO_NAME);

    if (mkfifo(WRITE_FIFO_NAME, 0644))
    {
        perror("ERROR\ncannot create the response pipe");
        exit(1);
    }

    if ((gFdRead = open(READ_FIFO_NAME, O_RDONLY)) == -1)
    {
        perror("ERROR\ncannot open the request pipe");
        exit(1);
    }

    if ((gFdWrite = open(WRITE_FIFO_NAME, O_WRONLY)) == -1)
    {
        perror("ERROR\ncannot open the response pipe");
        exit(1);
    }

    WriteString("CONNECT");

    printf("SUCCESS\n");
}

void Exit()
{
    close(gFdRead);

    close(gFdWrite);
    unlink(WRITE_FIFO_NAME);
}



void Ping()
{
    WriteString("PING");
    WriteString("PONG");
    WriteNumber(VARIANT);
}

int CreateShm(size_t* sharedMemorySize, int *shmID)
{
    *sharedMemorySize = ReadNumber();

    *shmID = shmget(SHARED_MEMORY_KEY, *sharedMemorySize, IPC_CREAT | 0664);
    if (*shmID == -1)
    {
        perror("ERROR\ncould not create shared memory");
        WriteString("CREATE_SHM");
        WriteString("ERROR");
        return 1;
    }

    gSharedZone = shmat(*shmID, NULL, 0);
    if (gSharedZone == (void*)-1)
    {
        perror("ERROR\ncould not attach to shared memory");
        WriteString("CREATE_SHM");
        WriteString("ERROR");
        return 1;
    }

    WriteString("CREATE_SHM");
    WriteString("SUCCESS");

    return 0;
}

int WriteToShm(unsigned int sharedMemorySize)
{
    if (gSharedZone == NULL)
    {
        perror("ERROR\nshared memory not created");
        WriteString("WRITE_TO_SHM");
        WriteString("ERROR");
        return 1;
    }

    unsigned int offset = ReadNumber();
    if (offset < 0 || offset >= sharedMemorySize)
    {
        WriteString("WRITE_TO_SHM");
        WriteString("ERROR");
        return 1;
    }

    unsigned int value = ReadNumber();
    if (offset + sizeof(value) >= sharedMemorySize)
    {
        WriteString("WRITE_TO_SHM");
        WriteString("ERROR");
        return 1;
    }

    memcpy(gSharedZone + offset, &value, sizeof(value));

    WriteString("WRITE_TO_SHM");
    WriteString("SUCCESS");

    return 0;
}

int MapFile(unsigned int* mappedFileSize)
{
    char* fileName = ReadString();

    int fd;
    if ((fd = open(fileName, O_RDONLY)) == -1)
    {
        perror("ERROR\nfailed to open the file");
        WriteString("MAP_FILE");
        WriteString("ERROR");
        return 1;
    }

    *mappedFileSize = lseek(fd, 0, SEEK_END);

    gMappedFile = mmap(NULL, *mappedFileSize, PROT_READ, MAP_PRIVATE, fd, 0);
    if (gMappedFile == (void*)-1)
    {
        perror("ERROR\nfailed to map the file");
        WriteString("MAP_FILE");
        WriteString("ERROR");
        return 1;
    }

    WriteString("MAP_FILE");
    WriteString("SUCCESS");

    free(fileName);

    return 0;
}

int ReadFromFileOffset(unsigned int mappedFileSize)
{
    if (gSharedZone == NULL)
    {
        perror("ERROR\nno shared memory region exists");
        WriteString("READ_FROM_FILE_OFFSET");
        WriteString("ERROR");
        return 1;
    }

    if (gMappedFile == NULL)
    {
        perror("ERROR\nno mapped file exists");
        WriteString("READ_FROM_FILE_OFFSET");
        WriteString("ERROR");
        return 1;
    }

    unsigned int offset = ReadNumber();
    unsigned int noBytes = ReadNumber();

    if (offset + noBytes >= mappedFileSize)
    {
        perror("ERROR\noversize");
        WriteString("READ_FROM_FILE_OFFSET");
        WriteString("ERROR");
        return 1;
    }

    memcpy(gSharedZone, gMappedFile + offset, noBytes);

    WriteString("READ_FROM_FILE_OFFSET");
    WriteString("SUCCESS");

    return 0;
}

int ReadFromFileSection(unsigned int mappedFileSize)
{
    if (gSharedZone == NULL)
    {
        perror("ERROR\nno shared memory region exists");
        WriteString("READ_FROM_FILE_SECTION");
        WriteString("ERROR");
        return 1;
    }

    if (gMappedFile == NULL)
    {
        perror("ERROR\nno mapped file exists");
        WriteString("READ_FROM_FILE_SECTION");
        WriteString("ERROR");
        return 1;
    }

    unsigned int sectionNr = ReadNumber();
    unsigned int offset = ReadNumber();
    unsigned int noBytes = ReadNumber();

    int headerSize = GetNumberFromMappedFile(mappedFileSize - 4, 2);
    int noSections = GetNumberFromMappedFile(mappedFileSize - headerSize + 1, 1);

    if (sectionNr > noSections)
    {
        perror("ERROR\nno such section");
        WriteString("READ_FROM_FILE_SECTION");
        WriteString("ERROR");
        return 1;
    }

    //(mappedFileSize - headerSize + 2) -> index of the first byte of the first section name;
    //... + (sectionNr - 1) * 22) -> index of the first byte of the desired section name;
    //... + 14 ->index of the first byte of the desired section offset.
    int sectionOffset = GetNumberFromMappedFile(mappedFileSize - headerSize + 2 + (sectionNr - 1) * 22 + 14, 4);

    memcpy(gSharedZone, gMappedFile + sectionOffset + offset, noBytes);

    WriteString("READ_FROM_FILE_SECTION");
    WriteString("SUCCESS");

    return 0;
}

int ReadFromLogicalSpaceOffset(unsigned int mappedFileSize)
{
    if (gSharedZone == NULL)
    {
        perror("ERROR\nno shared memory region exists");
        WriteString("READ_FROM_LOGICAL_SPACE_OFFSET");
        WriteString("ERROR");
        return 1;
    }

    if (gMappedFile == NULL)
    {
        perror("ERROR\nno mapped file exists");
        WriteString("READ_FROM_LOGICAL_SPACE_OFFSET");
        WriteString("ERROR");
        return 1;
    }

    unsigned int logicalOffset = ReadNumber();
    unsigned int noBytes = ReadNumber();


    int headerSize = GetNumberFromMappedFile(mappedFileSize - 4, 2);
    int noSections = GetNumberFromMappedFile(mappedFileSize - headerSize + 1, 1);

    int temp = 0;
    int offset = 0;
    int firstSectionOffsetIndex = mappedFileSize - headerSize + 2 + 14;
    for (int i = 0; i < noSections; i++)
    {
        int sectionOffset = GetNumberFromMappedFile(firstSectionOffsetIndex + i * 22, 4);
        int sectionSize = GetNumberFromMappedFile(firstSectionOffsetIndex + i * 22 + 4, 4);

        if (sectionSize % ALIGNMENT == 0)
        {
            temp = sectionSize / ALIGNMENT * ALIGNMENT;
        }
        else
        {
            temp = (sectionSize / ALIGNMENT + 1) * ALIGNMENT;
        }


        if (offset + temp >= logicalOffset)
        {
            offset = sectionOffset + logicalOffset - offset;
            break;
        }
        else
        {
            offset += temp;
        }

    }

    memcpy(gSharedZone, gMappedFile + offset, noBytes);

    WriteString("READ_FROM_LOGICAL_SPACE_OFFSET");
    WriteString("SUCCESS");

    return 0;
}



int main()
{
    Connect();

    int shmID = -1;
    size_t sharedMemorySize = 0;
    unsigned int mappedFileSize = 0;

    char numberBuffer[4];
    while (1)
    {
        memset(numberBuffer, 0, 4);
        char* request = ReadString();

        if (strcmp(request, "PING") == 0)
        {
            Ping();
        }
        else if (strcmp(request, "CREATE_SHM") == 0)
        {
            CreateShm(&sharedMemorySize, &shmID);
        }
        else if(strcmp(request, "WRITE_TO_SHM") == 0)
        {
            WriteToShm(sharedMemorySize);
        }
        else if (strcmp(request, "MAP_FILE") == 0)
        {
            MapFile(&mappedFileSize);
        }
        else if (strcmp(request, "READ_FROM_FILE_OFFSET") == 0)
        {
            ReadFromFileOffset(mappedFileSize);
        }
        else if (strcmp(request, "READ_FROM_FILE_SECTION") == 0)
        {
            ReadFromFileSection(mappedFileSize);
        }
        else if (strcmp(request, "READ_FROM_LOGICAL_SPACE_OFFSET") == 0)
        {
            ReadFromLogicalSpaceOffset(mappedFileSize);
        }
        else if (strcmp(request, "EXIT") == 0)
        {
            Exit();
            break;
        }
        else
        {
            printf("ERROR\nnot a valid request\n");
            break;
        }

        free(request);
    }

    shmdt(gSharedZone);
    shmctl(shmID, IPC_RMID, 0);
    gSharedZone = NULL;

    return 0;
}