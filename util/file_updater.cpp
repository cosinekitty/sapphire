#include <cstdio>
#include "file_updater.hpp"

int UpdateFile(const char *programName, const char *tempFileName, const char *keptFileName)
{
    // If the header file exists, and it contains the same text, don't do anything.
    FILE *file1 = fopen(keptFileName, "rb");
    FILE *file2 = fopen(tempFileName, "rb");
    bool identical = false;
    if (file1 && file2)
    {
        identical = true;
        int c1 = 0;
        while (c1 != EOF)
        {
            c1 = fgetc(file1);
            int c2 = fgetc(file2);
            if (c1 != c2)
            {
                identical = false;
                break;
            }
        }

    }
    if (file1) fclose(file1);
    if (file2) fclose(file2);

    if (identical)
    {
        // No need to do anything. The file already contains the text we want.
        // Delete the temporary file.
        remove(tempFileName);
        printf("%s: Kept: %s\n", programName, keptFileName);
    }
    else
    {
        if (rename(tempFileName, keptFileName))
        {
            printf("%s:  !!! cannot rename [%s] to [%s]\n", programName, tempFileName, keptFileName);
            return 1;
        }
        printf("%s: Wrote: %s\n", programName, keptFileName);
    }

    return 0;
}
