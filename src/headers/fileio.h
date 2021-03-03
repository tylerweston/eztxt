#ifndef MIPSZE_FILEIO
#define MIPSZE_FILEIO

int load_doc(const char* filename, doc* document);
void save_doc(const char* filename, doc* document);
int check_file_exists(const char* filename);

#endif