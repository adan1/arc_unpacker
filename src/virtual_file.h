#ifndef VIRTUAL_FILE_H
#define VIRTUAL_FILE_H
#include <string>
#include "io.h"

class VirtualFile final
{
public:
    VirtualFile(const std::string path, const std::string mode);
    VirtualFile();
    ~VirtualFile();
    IO &io;
    std::string name;
    void change_extension(const std::string new_extension);
};

#endif