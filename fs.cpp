#include <iostream>
#include <algorithm>
#include <fstream>
#include <cstring>
#include <vector>
#include <iomanip>
#include <cmath>
#include <math.h>
#include <list>
#include "fs.h"
#include "disk.h"

#define BLOCK_SIZE 4096

FS::FS()
{
    std::cout << "FS::FS()... Creating file system\n";
}

FS::~FS()
{

}

std::string path_pwd = "";
int current_position = 0;

std::list<uint16_t> find_empty_fat_block(int amount); 
std::pair<uint16_t, bool> find_current_directory(std::string path);

// utils
std::pair<std::string, std::string> split_file_name(const std::string& str) {
    std::size_t found = str.find_last_of("/");
    return std::make_pair(str.substr(found + 1), str.substr(0,found + 1));
}

std::pair<uint16_t, bool>
FS::find_current_directory(std::string path, bool add) {
    /*
     * This function finds the current directory for given path.
     * Returns a pair with (block position for current directory, success) 
     */

    size_t pos = 0;
    uint16_t position = 0;

    std::string token;
    std::string delimiter = "/";

    dir_entry d_entries[BLOCK_SIZE / sizeof(dir_entry)];

    bool found = false; 

    if(add) {
        auto t = split_file_name(path);
        path = std::get<1>(t);
        std::string name = std::get<0>(t);

        if(path == name) {
            return std::make_pair(position, true);
        }
    }

    if (path.size() != 0) {
        std::string tmp;
        tmp = path;

        // Split path by delimiter
        while((pos = tmp.find(delimiter)) != std::string::npos) {
            found = false;
            token = tmp.substr(0, pos);
            tmp.erase(0, pos + delimiter.length());

            disk.read(position ,(uint8_t*)d_entries); 

            for(int i = 0; i < (BLOCK_SIZE / sizeof(dir_entry)); i++) {
                if(strcmp(token.c_str(), d_entries[i].file_name) == 0) {
                    position = d_entries[i].first_blk;
                    if(d_entries[i].type == 0) {
                        std::cout << "ERROR: can't cd to file" << std::endl;
                        return std::make_pair(position, found); 
                    } else {
                        found = true;
                        if(token == "..") { 
                            // .. means that we go to parent directory
                            auto split = split_file_name(path_pwd);
                            path_pwd = std::get<1>(split);

                            position = d_entries[0].first_blk;
                        } else {
                            // We go deeper into directories
                            position = d_entries[i].first_blk;
                        }
                    }
                }
            }
            
            // If we didn't find directory, then we return FAILURE
            if(!found) {
                return std::make_pair(position, found);
            }
        }
        
        std::cout << tmp  << std::endl;
        if (tmp.find("/") != std::string::npos) {
            // found
        } else {
            disk.read(position, (uint8_t*)d_entries); 
                
            for(int i = 0; i < (BLOCK_SIZE / sizeof(dir_entry)); i++) {
                    
                if(strcmp(tmp.c_str(), d_entries[i].file_name) == 0) {
                    if(d_entries[i].type == 1) {
                        found = true;
                        position = d_entries[i].first_blk;
                    }
                }
            }
        }
    } 

    return std::make_pair(position, true);
}

std::pair<dir_entry, bool> 
FS::find_file(std::string filepath) {
    /*
     * Checks if the file exists for given filepath
     * Returns a pair of (dir_entry, success)
     */
    dir_entry file;
    dir_entry d_entries[BLOCK_SIZE / sizeof(dir_entry)];
   
    std::cout << filepath << std::endl; 
    
    auto t = find_current_directory(filepath, true);
    uint16_t position = std::get<0>(t);
    bool success_t = std::get<1>(t);

    // If position is not root
    auto split = split_file_name(filepath);
    std::string name = std::get<0>(split);

    bool found_file = false;

    if(!success_t) {
        return std::make_pair(file, found_file);
    }

    disk.read(position, (uint8_t*)d_entries);

    for(int i = 0; i < (BLOCK_SIZE / sizeof(dir_entry)); i++) {

        if(strcmp(d_entries[i].file_name, name.c_str()) == 0) {

            file = d_entries[i];
            found_file = true;
            break;
        }
    }

    return std::make_pair(file, found_file);
}

std::tuple<bool, std::list<uint16_t>, uint16_t> 
FS::find_empty_dir_block(std::string pathname, uint32_t size, int type, int nr_blocks) {
    /*
     * Tries to find a empty dir_block and return tuple with (Success, list with fat_blocks, dir_entry)
     */
    dir_entry d_entries[BLOCK_SIZE / sizeof(dir_entry)];

    bool success = false;
    std::list<uint16_t> f_entries;
    uint16_t position;

    auto t = find_current_directory(pathname, true);
    bool success_t = std::get<1>(t); 
    
    // Return if the path is invalid path
    if(!success_t) {
        return std::make_tuple(success, f_entries, position);    
    }

    position = std::get<0>(t);
    disk.read(position, (uint8_t*)d_entries);

    auto split = split_file_name(pathname);
    std::string name = std::get<0>(split);
        
    dir_entry new_dir;
    strcpy(new_dir.file_name, name.c_str());

    // Allocates all fat block we need
    f_entries = find_empty_fat_block(static_cast<int>(nr_blocks));

    if(f_entries.size() == 0) {
        return std::make_tuple(success, f_entries, position);
    }

    new_dir.first_blk = f_entries.front();
    new_dir.size = size; 
    new_dir.type = type;
    new_dir.access_rights = 0x06;

    // find free entry
    for(int i = 0; i < (BLOCK_SIZE / sizeof(dir_entry)); i++) {
        std::string tmp(d_entries[i].file_name);
        if(strcmp(d_entries[i].file_name, "") == 0) {
        
            if(tmp.size() == 0) {
                d_entries[i] = new_dir; 
                success = true;
                break;
            } 
        }
    }

    disk.write(position, (uint8_t*)d_entries);

    return std::make_tuple(success, f_entries, position);
}

std::list<uint16_t>
FS::find_empty_fat_block(int amount) {
    /*
     * Tries to find empty fat blocks.
     * Returns a list with all allocated fat blocks.
     */

    int16_t f_entries[BLOCK_SIZE/2];
    std::list<uint16_t> f_pointers;

    disk.read(1, (uint8_t*)f_entries);
    
    // We need to know the FAT BEFORE so we can link it to the new FAT!
    int16_t tmp_fat_before = 0;

    // Setup fat link
    for (int i = 0; i < BLOCK_SIZE/2; i++) {
        if (f_entries[i] == 0) {

           if(f_pointers.size() == 0) {
               // First fat block
               tmp_fat_before = i;
               f_pointers.push_back(i);
               f_entries[i] = -1;           
               
               if(f_pointers.size() == amount) {
                    // In the case we only need 1 block
                    break;
               }
               continue;
           } else if (f_pointers.size() != amount) {
               f_entries[tmp_fat_before] = i;
               f_pointers.push_back(i);
               f_entries[i] = -1;
               tmp_fat_before = i;
               continue;
           } else {
               // We found all fat blocks we need!
               break;
           }
        }
    }

    if (f_pointers.size() != amount) {
        std::cout << "ERROR: FULL FAT" << std::endl;
    } else {  
        disk.write(1, (uint8_t*)f_entries);
    }

    return f_pointers;
}

// formats the disk, i.e., creates an empty file system
int
FS::format()
{
    std::cout << "FS::format()\n";

    int16_t f_entries[BLOCK_SIZE/2];

    // Format the fat entries.
    for (int i = 0; i < BLOCK_SIZE/2; i++) {
    	if (i == 0) {
	    	// Root directory
		    f_entries[i] = -1;
    	} else if (i == 1) {
            // FAT
            f_entries[i] = -1;	
	    } else {
            // Set all other to free
            f_entries[i] = 0;
        }
    }   
	  
    disk.write(1, (uint8_t*)f_entries);
    return 0;
}

// create <filepath> creates a new file on the disk, the data content is
// written on the following rows (ended with an empty row)
int
FS::create(std::string filepath)
{
    std::cout << "FS::create(" << filepath << ")\n";
    std::string complete_path;

    if(filepath.at(0) == '/') {
        filepath.erase(0, 1);
        complete_path = filepath; 
    } else {
        complete_path = path_pwd + filepath;
    }

    auto file_t = find_file(complete_path);
    bool success_t = std::get<1>(file_t);

    if(success_t) {
        std::cout << "ERROR: name already used!" << std::endl;
        return 1;
    }

    auto split = split_file_name(complete_path);
    std::string name = std::get<0>(split);
        
    if(name.size() > 52) {
        std::cout << "Too long filename" << std:: endl;
        return 1;
    }
    
    std::string data;
    std::string tmp_data;

    // Load data from terminal
    do {
        getline(std::cin, tmp_data);
        data.append(tmp_data);
    } while (tmp_data != "");

    // Calculate the nr of fat blocks we need.
    uint32_t s_data = data.length();

    float l_data = s_data;
    float s_block = BLOCK_SIZE;

    float calc = (l_data / s_block);
    float nr_blocks = std::ceil(calc);

    if(data == "") {
        nr_blocks = 1;        
    }
   
    auto dir_t = find_empty_dir_block(complete_path, s_data, 0, nr_blocks);
    bool dir_success = std::get<0>(dir_t);
    std::list<uint16_t> f_entries = std::get<1>(dir_t); 

    if(!dir_success) {
        std::cout << "Could not create file: " << complete_path << std::endl;
        return 1;
    }

    /*
    for(std::list<uint16_t>::iterator it = f_entries.begin(); it != f_entries.end(); it++) {
        std::cout << "Number of PAT's" << std::endl;
        std::cout << *it << std::endl;
    }*/

    std::string tmp = data;
    char buffer[BLOCK_SIZE];
    std::list<uint16_t>::iterator it = f_entries.begin();

    // Write data to disk
    for(int i = 0; i < nr_blocks; i++) {
        if (tmp.length() < BLOCK_SIZE) {
            // one block
            std::strcpy(buffer, tmp.c_str());
            disk.write(*it, (uint8_t*)buffer);
        } else {
            // multiple blocks
            std::string d_write = tmp.substr(0, BLOCK_SIZE -1);
            std::strcpy(buffer, d_write.c_str());
            disk.write(*it, (uint8_t*)buffer);
            tmp.erase(0, BLOCK_SIZE);
            std::advance(it, 1);
        }
    }

    return 0;
}


// cat <filepath> reads the content of a file and prints it on the screen
int
FS::cat(std::string filepath)
{
    std::cout << "FS::cat(" << filepath << ")\n";

    std::string complete_path;

    if(filepath.at(0) == '/') {
        filepath.erase(0, 1);
        complete_path = filepath; 
    } else {
        complete_path = path_pwd + filepath;
    }

    auto t = find_file(complete_path);

    dir_entry file = std::get<0>(t);
    bool success = std::get<1>(t);
    
    if(!success) {
        std::cout << "ERROR: couldn't find file " << filepath << std::endl;
        return 1;
    }

    if(file.type == 1) {
        std::cout << "ERROR: can't read dict" << std::endl;
    }

    // Check if we have permissions to read
    int access = (int)file.access_rights;
    std::cout << "Access: " << access << std::endl;
    
    if(access == (0x04) || access == (0x04 | 0x02) || access == (0x04 | 0x01)) {
        // ok access good
    } else {
        std::cout << "ERROR: no permission to read file" << std::endl;
        return 1;    
    }
    
    uint16_t f_entries[BLOCK_SIZE / sizeof(uint16_t)];

    disk.read(1, (uint8_t*)f_entries);

    std::list<uint16_t> f_file_entries;

    // First FAT block for file
    int16_t tmp = file.first_blk;

    // Find rest of the FAT's
    while(tmp != -1) {
        f_file_entries.push_back(tmp);
        tmp = f_entries[tmp];
    }   

    char data[BLOCK_SIZE];
    std::string data_to_print;

    // Read all data for given file
    for(std::list<uint16_t>::iterator it = f_file_entries.begin(); it != f_file_entries.end(); it++) {
        disk.read(*it, (uint8_t*)data);

        data_to_print += data;
    } 

    std::cout << data_to_print << std::endl;
    return 0;
}

// ls lists the content in the currect directory (files and sub-directories)
int
FS::ls()
{
    std::cout << "FS::ls()\n";

    dir_entry d_entries[BLOCK_SIZE / sizeof(dir_entry)];

    std::cout << "Current pwd: " << path_pwd << std::endl;

    // Locate pwd directory
    auto t = find_current_directory(path_pwd, false);
    bool success_t = std::get<1>(t);

    if(!success_t) {
        // This should never be able to happen, but for safety we keep it!
        std::cout << "ERROR: couldn't find path pwd" << std::endl;
        return 1;
    }
    
    // Fat block for current directory 
    uint16_t position = std::get<0>(t);

    disk.read(position, (uint8_t*)d_entries);
    std::list<dir_entry> content;

    for(int i = 0; i < (BLOCK_SIZE / sizeof(dir_entry)); i++) {
        std::string tmp(d_entries[i].file_name);

        if(tmp.size() != 0) {
            if(d_entries[i].type == 0 || d_entries[i].type == 1) {
                content.push_back(d_entries[i]);
            }
        }
    }
   
    for(std::list<dir_entry>::iterator it = content.begin(); it != content.end(); it++) {
        std::string type_name;

        if((*it).type ==0) {
            type_name = "file";
        } else {
            type_name = "dir";
        }
    
        std::string access_rights;

        if((*it).access_rights == (0x04 | 0x02)) {
            access_rights = "rw-";
        } else if ((*it).access_rights == (0x01)) {
            access_rights = "--x";
        } else if ((*it).access_rights == (0x02)) {
            access_rights = "-w-";
        } else if ((*it).access_rights == (0x04)) {
            access_rights = "r--";
        } else if ((*it).access_rights == (0x01 | 0x02)) {
            access_rights = "-wx";
        } else if ((*it).access_rights == (0x04 | 0x01)) {
            access_rights = "r-x";
        } else {
            access_rights = "rwx";
        }

        std::cout << std::left << std::setw(20) << (*it).file_name;
        std::cout << std::setw(20) << type_name << std::setw(28) << access_rights;

        std::cout << std::right << (*it).size << std::endl; 
    } 

    return 0;
}

// cp <sourcepath> <destpath> makes an exact copy of the file
// <sourcepath> to a new file <destpath>
int
FS::cp(std::string sourcepath, std::string destpath)
{
    std::cout << "FS::cp(" << sourcepath << "," << destpath << ")\n";

    std::string complete_path_source;

    if(sourcepath.at(0) == '/') {
        sourcepath.erase(0, 1);
        complete_path_source = sourcepath; 
    } else {
        complete_path_source = path_pwd + sourcepath;
    }


    // Find file to copy
    auto t = find_file(complete_path_source);
    bool success = std::get<1>(t);

    if(!success) {
        std::cout << "ERROR: Could not find file! -> " << complete_path_source << std::endl;
        return 1;
    }

    dir_entry file_to_cpy = std::get<0>(t);

    std::string complete_path_dest;

    if(destpath.at(0) == '/') {
        destpath.erase(0, 1);
        complete_path_dest = destpath; 
    } else {
        complete_path_dest = path_pwd + destpath;
    }

    // Check that file with that name does not already exist 
    auto g = find_file(complete_path_dest);
    bool success_g = std::get<1>(g);
    dir_entry file_g = std::get<0>(g);

    bool new_file_name = true;

    if(success_g && file_g.type == 0) {
        std::cout << "ERROR: A file with that name already exists " << destpath << std::endl;
        return 1;
    } else {
        new_file_name = false;
    }
    
    std::string name;

    if(new_file_name) {
        auto split = split_file_name(complete_path_dest);
        name = std::get<0>(split);
    } else {
        auto split_g = split_file_name(complete_path_source);  
        name = std::get<0>(split_g);
        complete_path_dest += "/";
        complete_path_dest += name;
        
        g = find_file(complete_path_dest);
        success_g = std::get<1>(g);

        if(success_g) {
            std::cout << "ERROR: A file with that name already exists " << destpath << std::endl;
            return 1;
        }
    }
        
    if(name.size() > 52) {
        std::cout << "Too long filename" << std:: endl;
        return 1;
    }

    bool empty_file = true;

    // Calculate nr of blocks
    float l_data = file_to_cpy.size;
    float s_block = BLOCK_SIZE;

    float calc = (l_data / s_block);
    float nr_blocks = std::ceil(calc);

    auto d = find_empty_dir_block(complete_path_dest, file_to_cpy.size, file_to_cpy.type, nr_blocks);
    bool success_d = std::get<0>(d);
    
    if(!success_d) {
        std::cout << "ERROR: Could not find empty dir block" << std::endl;
        return 1;
    }

    // Newly allocated fat blocks for file
    std::list<uint16_t> f_new_entries = std::get<1>(d);

    if(f_new_entries.size() != nr_blocks) {
        std::cout << "Couldn't find empty fat blocks" << std::endl;
        return 1;
    }

    uint16_t f_entries[BLOCK_SIZE / sizeof(uint16_t)];

    disk.read(1, (uint8_t*)f_entries);

    std::list<uint16_t> f_old_entries;
    int16_t tmp = file_to_cpy.first_blk;

    // Find FAT's for old file.
    while(tmp != -1) {
        f_old_entries.push_back(tmp);
        tmp = f_entries[tmp];
    }   

    char data[BLOCK_SIZE];

    std::list<uint16_t>::iterator it_old = f_old_entries.begin();
    std::list<uint16_t>::iterator it_new = f_new_entries.begin();

    // Copy data
    for(int i = 0; i < nr_blocks; i++) {
        disk.read(*it_old, (uint8_t*)data);
        std::cout << *it_old << std::endl;
        std::cout << "Data: " << data << std::endl;
        disk.write(*it_new, (uint8_t*)data);
        std::advance(it_old, 1);
        std::advance(it_new, 1);
    }

    return 0;
}

// mv <sourcepath> <destpath> renames the file <sourcepath> to the name <destpath>,
// or moves the file <sourcepath> to the directory <destpath> (if dest is a directory)
int
FS::mv(std::string sourcepath, std::string destpath)
{
    std::cout << "FS::mv(" << sourcepath << "," << destpath << ")\n";

    std::string complete_path_source;

    if(sourcepath.at(0) == '/') {
        sourcepath.erase(0, 1);
        complete_path_source = sourcepath; 
    } else {
        complete_path_source = path_pwd + sourcepath;
    }

    // First of all we need to find the file to move.
    auto t = find_file(complete_path_source);
    bool success_t = std::get<1>(t);
    dir_entry file_t = std::get<0>(t);

    if(!success_t) {
        std::cout << "ERROR: Could not find file! -> " << sourcepath << std::endl;
        return 1;
    }

    if(file_t.type == 1) {
        std::cout << "ERROR: Can't move directory." << std::endl;
        return 1;
    }
    
    // Find directory for source_path  
    auto source_dir = find_current_directory(complete_path_source, true);
    bool success_source_dir = std::get<1>(t); 

    uint16_t source_position = std::get<0>(source_dir);

    if(!success_source_dir) {
        std::cout << "ERROR: Invalid path" << std::endl; 
        return 1;
    }


    std::string complete_path_dest;

    // We found the file that we need to move
    // Now lets check that the destpath exists
    if(destpath.at(0) == '/') {
        destpath.erase(0, 1);
        complete_path_dest = destpath; 
    } else {
        complete_path_dest = path_pwd + destpath;
    }

    // If check that there does not already exists a file with that filename
    auto g = find_file(complete_path_dest);
    bool success_g = std::get<1>(g);
    dir_entry file_g = std::get<0>(g);

    std::cout << file_g.type << std::endl;
   
    bool new_file_name = true;

    if(success_g && file_g.type == 0) {
        std::cout << "ERROR: There already exists a file with that name " << destpath << std::endl;
        return 1;
    } else {
        new_file_name = false;
    }

    std::string name;

    if(new_file_name) {
        auto split = split_file_name(complete_path_dest);
        name = std::get<0>(split);
    } else {
        auto split_g = split_file_name(complete_path_source);  
        name = std::get<0>(split_g);

        // Check if file exists
        std::string path_dest_tmp = complete_path_dest; 
        path_dest_tmp += "/";
        path_dest_tmp += name;

        g = find_file(path_dest_tmp);
        success_g = std::get<1>(g);

        if(success_g) {
            std::cout << "ERROR: A file with that name already exists " << destpath << std::endl;
            return 1;
        }
 
    }
        
    if(name.size() > 52) {
        std::cout << "Too long filename" << std:: endl;
        return 1;
    }


    // Find directory for dest_path
    auto dest_dir = find_current_directory(complete_path_dest, new_file_name);
    bool success_dest_dir = std::get<1>(dest_dir); 
    
    if(!success_dest_dir) {
        std::cout << "ERROR: Invalid path" << std::endl; 
        return 1;
    }

    uint16_t dest_position = std::get<0>(dest_dir);

    auto split_source = split_file_name(complete_path_source);
    std::string name_source = std::get<0>(split_source);

    if(source_position != dest_position) {
        // If not same folder remove dir_entry from source
        dir_entry d_entries_source[BLOCK_SIZE / sizeof(dir_entry)];
        disk.read(source_position, (uint8_t*)d_entries_source);

        for(int i = 0; i < (BLOCK_SIZE / sizeof(dir_entry)); i++) {
            if(strcmp(d_entries_source[i].file_name, name_source.c_str()) == 0) {
                strcpy(d_entries_source[i].file_name, "");
                d_entries_source[i].first_blk = 0;
                d_entries_source[i].size = 0;
            }
        }
        disk.write(source_position, (uint8_t*)d_entries_source);
    }

    dir_entry d_entries_dest[BLOCK_SIZE / sizeof(dir_entry)];
    disk.read(dest_position, (uint8_t*)d_entries_dest);

    // auto split_dest = split_file_name(destpath);
    // std::string name_dest = std::get<0>(split_dest);
    
    for(int i = 0; i < (BLOCK_SIZE / sizeof(dir_entry)); i++) {
        if(source_position == dest_position) {
            if(strcmp(d_entries_dest[i].file_name, name_source.c_str()) == 0) {
                strcpy(file_t.file_name, name.c_str());
                d_entries_dest[i] = file_t;
                break;
            }
        } else {
            if(strcmp(d_entries_dest[i].file_name, "") == 0) {
                if(d_entries_dest[i].first_blk == 0) {
                    strcpy(file_t.file_name, name.c_str());
                    d_entries_dest[i] = file_t;
                    break;
                }
            }
        }
    } 

    disk.write(dest_position, (uint8_t*)d_entries_dest);
    
    return 0;
}

// rm <filepath> removes / deletes the file <filepath>
int
FS::rm(std::string filepath)
{
    std::cout << "FS::rm(" << filepath << ")\n";

    std::string complete_path;

    if(filepath.at(0) == '/') {
        filepath.erase(0, 1);
        complete_path = filepath; 
    } else {
        complete_path = path_pwd + filepath;
    }

    auto g = find_file(complete_path);
    bool success_g = std::get<1>(g);
    dir_entry file_g = std::get<0>(g);

    if(!success_g) {
        std::cout << "ERROR: Could not find file! -> " << complete_path << std::endl;
        return 1;
    }
    
    /*
    if(file.access_rights != 0x04 || file.access_rights != (0x02 | 0x04) || file.access_rights == (0x04 | 0x01)) {
        std::cout << "ERROR: no permission to read file" << std::endl;
        return 1;        
    }
    */
    
    auto dir_g = find_current_directory(complete_path, true);
    bool success_dir_g = std::get<1>(dir_g); 
    
    if(!success_dir_g) {
        std::cout << "ERROR: Invalid path" << std::endl; 
        return 1;
    }

    uint16_t position_g = std::get<0>(dir_g);

    int nr_files = 0;

    // Check if directory is empty
    if(file_g.type == 1) {
        dir_entry d_entries_check[BLOCK_SIZE / sizeof(dir_entry)];
        disk.read(file_g.first_blk, (uint8_t*)d_entries_check);

        for(int i = 1; i < (BLOCK_SIZE / sizeof(dir_entry)); i++) {
            if(strcmp(d_entries_check[i].file_name, "") != 0) {
                if(d_entries_check[i].first_blk != 0) {
                    ++nr_files;
                }
            }
        }
    }

    if (nr_files != 0) {
        std::cout << "ERROR: can't delete directory with files" << std::endl;
        return 1;
    }

    auto split = split_file_name(complete_path);
    std::string name = std::get<0>(split);

    dir_entry d_entries[BLOCK_SIZE / sizeof(dir_entry)];
    
    disk.read(position_g, (uint8_t*)d_entries);
    
    // Find and remove dict
    for(int i = 0; i < (BLOCK_SIZE / sizeof(dir_entry)); i++) {
        if(strcmp(d_entries[i].file_name, name.c_str()) == 0) {
            strcpy(d_entries[i].file_name, "");
            d_entries[i].first_blk = 0;
            d_entries[i].size = 0;
            break;
        }        
    }

    disk.write(position_g, (uint8_t*)d_entries);

    // We need to free PAT as well.
    uint16_t f_entries[BLOCK_SIZE / sizeof(uint16_t)];

    disk.read(1, (uint8_t*)f_entries);

    int16_t tmp = file_g.first_blk;

    while(tmp != -1) {
        int16_t y = f_entries[tmp];
        f_entries[tmp] = 0;
        tmp = y;
    }

    f_entries[tmp] = 0;

    disk.write(1, (uint8_t*)f_entries);
    return 0;
}

// append <filepath1> <filepath2> appends the contents of file <filepath1> to
// the end of file <filepath2>. The file <filepath1> is unchanged.
int
FS::append(std::string filepath1, std::string filepath2)
{
    std::cout << "FS::append(" << filepath1 << "," << filepath2 << ")\n";
    std::string complete_path;

    if(filepath1.at(0) == '/') {
        filepath1.erase(0, 1);
        complete_path = filepath1; 
    } else {
        complete_path = path_pwd + filepath1;
    }

    // First of all we check that filepath1 exists 
    // TODO: Handle if file is in other katalogs
    auto t = find_file(complete_path);
    bool success_t = std::get<1>(t);
    dir_entry file_t = std::get<0>(t);

    if(!success_t) {
        std::cout << "ERROR: Could not find file! -> " << filepath1 << std::endl;
        return 1;
    }

    if(file_t.type == 1) {
        std::cout << "ERROR: Can't append directory." << std::endl;
        return 1;
    }
    
    int access_t = (int)file_t.access_rights;
    
    if(access_t == (0x04) || access_t == (0x04 | 0x02) || access_t == (0x04 | 0x01)) {
        // ok access good
    } else {
        std::cout << "ERROR: no permission to read file" << std::endl;
        return 1;    
    }
    
    if(filepath2.at(0) == '/') {
        filepath2.erase(0, 1);
        complete_path = filepath2; 
    } else {
        complete_path = path_pwd + filepath2;
    }

    // Found filepath1
    // Check filepath2
    auto g = find_file(complete_path);
    bool success_g = std::get<1>(g);
    dir_entry file_g = std::get<0>(g);

    if(!success_g) {
        std::cout << "ERROR: Could not find file! -> " << filepath2 << std::endl;
        return 1;
    }

    int access_g = (int)file_g.access_rights;
    
    if(access_g == (0x02) || access_g == (0x04 | 0x02) || access_g == (0x02 | 0x01)) {
        // ok access good
    } else {
        std::cout << "ERROR: no permission to write to file" << std::endl;
        return 1;   
    }
    
    if(file_g.type == 1) {
        std::cout << "ERROR: Can't append directory." << std::endl;
        return 1;
    }

    // Find source_path directory
    auto dir_g = find_current_directory(complete_path, true);
    bool success_dir_g = std::get<1>(dir_g); 
    
    if(!success_dir_g) {
        std::cout << "ERROR: Invalid path" << std::endl; 
        return 1;
    }

    uint16_t position_g = std::get<0>(dir_g);

    uint16_t f_entries[BLOCK_SIZE / sizeof(uint16_t)];
    disk.read(1, (uint8_t*)f_entries);

    std::list<uint16_t> f_t_entries;
    int16_t tmp_t = file_t.first_blk;

    // Find PAT's for first file
    while(tmp_t != -1) {
        f_t_entries.push_back(tmp_t);
        tmp_t = f_entries[tmp_t];
    }   

    std::list<uint16_t> f_g_entries;
    int16_t tmp_g = file_g.first_blk;

    // Find PAT's for second file
    while(tmp_g != -1) {
        f_g_entries.push_back(tmp_g);
        tmp_g = f_entries[tmp_g];
    }   

    int diskspace_left = BLOCK_SIZE - (file_g.size % BLOCK_SIZE);  

    char data_t[BLOCK_SIZE]; // first file
    char data_g[BLOCK_SIZE]; // second file 

    if(file_t.size <= diskspace_left) {
        // We dont need more blocks
       
        auto it_t = f_t_entries.end();
        it_t--;
        disk.read(*it_t, (uint8_t*)data_t);

        auto it_g = f_g_entries.end();
        it_g--;
        disk.read(*it_g, (uint8_t*)data_g);

        char buffer[4096];
        //std::cout << data_t << std::endl;
        //std::cout << data_g << std::endl;
        std::string tmp;
        std::string newline = "\n";

        // change order?
        tmp.append(data_g);
        tmp.append(newline);
        tmp.append(data_t);

        strcpy(buffer, tmp.c_str());

        disk.write(*it_g, (uint8_t*)buffer);

    } else {
        // We need more blocks
        
        // Read last block of last file.
        auto it_g = f_g_entries.end();
        it_g--;
        disk.read(*it_g, (uint8_t*)data_g);

        // Add last block to data
        std::string tmp_data;
        tmp_data += '\n';
        tmp_data += data_g;

        // Gather all the data from file1 and append to tmp_data
        for(std::list<uint16_t>::iterator it = f_t_entries.begin(); it != f_t_entries.end(); it++) {
            disk.read(*it, (uint8_t*)data_t);
            tmp_data += data_t;
        }    

        // Now we have all data that we need.
        // Since we already have a block from file1, we need 1 block less
        float size_left = tmp_data.size() - BLOCK_SIZE;
        float size_blk = BLOCK_SIZE;
        
        // Calculate how many blocks we need to allocate 
        int nr_blocks = std::ceil(size_left / size_blk);
        
        // New fat entries
        std::list<uint16_t> f_new_entries = find_empty_fat_block(nr_blocks); 

        // Link file_t to new fat entries
        std::list<uint16_t>::iterator it_new = f_new_entries.begin();

        // Lets check that there is space left to execute append
        if(f_new_entries.size() != nr_blocks) {
            std::cout << "ERROR: Couldn't locate enough fat block" << std::endl;
            return 1;
        }

        // However we need to re-read, since we updated fat after allocating new fats
        disk.read(1, (uint8_t*)f_entries);
        f_entries[*it_g] = *it_new;   

        // Update fat!
        disk.write(1, (uint8_t*)f_entries);

        // Write to the last block in second file.
        char buffer[BLOCK_SIZE];
        std::string d_write = tmp_data.substr(0, BLOCK_SIZE -1);
        strcpy(buffer, d_write.c_str()); 
        disk.write(*it_g, (uint8_t*)buffer);
        tmp_data.erase(0, BLOCK_SIZE);

        // Write to newly allocate fatblocks
        for(int i = 0; i < nr_blocks; i++) {
            std::string d_write = tmp_data.substr(0, BLOCK_SIZE -1);
            strcpy(buffer, d_write.c_str()); 
            disk.write(*it_new, (uint8_t*)buffer);
            tmp_data.erase(0, BLOCK_SIZE);
            std::advance(it_new, 1);
        }
    }

    // Update size for dir_entry
    dir_entry d_entries[BLOCK_SIZE / sizeof(dir_entry)];
    disk.read(position_g, (uint8_t*)d_entries);
    
    for(int i = 0; i < (BLOCK_SIZE / sizeof(dir_entry)); i++) {
        if(strcmp(file_g.file_name, d_entries[i].file_name) == 0) {
            d_entries[i].size = file_g.size + file_t.size;
        }
    }

    disk.write(position_g, (uint8_t*)d_entries);

    return 0;
}

// mkdir <dirpath> creates a new sub-directory with the name <dirpath>
// in the current directory
int
FS::mkdir(std::string dirpath)
{
    std::cout << "FS::mkdir(" << dirpath << ")\n";

    std::string complete_path;
     
    if(dirpath.at(0) == '/') {
        dirpath.erase(0, 1);
        complete_path = dirpath; 
    } else {
        complete_path = path_pwd + dirpath;
    }

    // Check that a file/dir with that name doesn't exists.
    auto file_t = find_file(complete_path);
    bool success_t = std::get<1>(file_t);

    if(success_t) {
        std::cout << "ERROR: name already used!" << std::endl;
        return 1;
    }

    auto dir_t = find_empty_dir_block(complete_path, 0, 1, 1);
    bool dir_success = std::get<0>(dir_t);
    std::list<uint16_t> f_entries = std::get<1>(dir_t); 

    if(!dir_success) {
        std::cout << "ERROR: could not create directory: " << dirpath << std::endl;
        return 1;
    }

    std::list<uint16_t>::iterator it_t = f_entries.begin();

    // Create sub_dir
    dir_entry sub_dir;
    strcpy(sub_dir.file_name, "..");

    uint16_t position = std::get<2>(dir_t); 

    sub_dir.first_blk = position;
    sub_dir.size = 0;   
    sub_dir.type = 1;
    sub_dir.access_rights = 0x06;

    dir_entry d_entries[BLOCK_SIZE / sizeof(dir_entry)];
    
    disk.read(*it_t, (uint8_t*)d_entries);

    dir_entry temp;
    strcpy(temp.file_name, "");
    temp.first_blk;
    temp.size = 0;
    temp.type = 0;

    // Reset new dict folder
    for(int i = 1; i < (BLOCK_SIZE / sizeof(dir_entry)); i++) {
        d_entries[i] = temp;
    }

    d_entries[0] = sub_dir;

    disk.write(*it_t, (uint8_t*)d_entries);

    return 0;
}

// cd <dirpath> changes the current (working) directory to the directory named <dirpath>
int
FS::cd(std::string dirpath)
{
    std::cout << "FS::cd(" << dirpath << ")\n";

    std::string complete_path;
    
    // Go to root    
    if(strcmp(dirpath.c_str(), "/") == 0) {
        path_pwd = "";        
        return 0;
    }
    
    if(dirpath.at(0) == '/') {
        dirpath.erase(0, 1);
        complete_path = dirpath + "/"; 
    } else {
        complete_path = path_pwd + dirpath + "/";
    }

    // Go to root    
    if(strcmp(dirpath.c_str(), "/") == 0) {
        path_pwd = "";        
        return 0;
    }

    auto t = find_current_directory(complete_path, false);
    bool success_t = std::get<1>(t);

    if(!success_t) {
        std::cout << "Error: Can't path to " << complete_path << std::endl;
        return 1;
    }
    
    size_t pos = 0;

    std::string delimiter = "/";
    std::string token;
    
    path_pwd = "";

    // find new path
    while((pos = complete_path.find(delimiter)) != std::string::npos) {
        token = complete_path.substr(0, pos);
        complete_path.erase(0, pos + delimiter.length());

        if(token != "..") {
            path_pwd = path_pwd + token + '/';
        } else {
            path_pwd[path_pwd.size() - 1] = ' ';
            auto split = split_file_name(path_pwd);

            path_pwd = std::get<1>(split);
        }
    } 
    
    std::cout << dirpath << std::endl;

    return 0;
}

// pwd prints the full path, i.e., from the root directory, to the current
// directory, including the currect directory name
int
FS::pwd()
{
    std::cout << "FS::pwd()\n";

    if(path_pwd.size() == 0) {
        std::cout << "/" << std::endl;
    } else {
        std::cout << "/" << path_pwd << std::endl;
    }
    return 0;
}

// chmod <accessrights> <filepath> changes the access rights for the
// file <filepath> to <accessrights>.
int
FS::chmod(std::string accessrights, std::string filepath)
{
    std::cout << "FS::chmod(" << accessrights << "," << filepath << ")\n";

    std::string complete_path;

    if(filepath.at(0) == '/') {
        filepath.erase(0, 1);
        complete_path = filepath; 
    } else {
        complete_path = path_pwd + filepath;
    }

    // file exist
    auto g = find_file(complete_path);
    bool success_g = std::get<1>(g);
    dir_entry file_g = std::get<0>(g);

    if(!success_g) {
        std::cout << "ERROR: Could not find file! -> " << complete_path << std::endl;
        return 1;
    }

    // find directory
    auto dir_g = find_current_directory(complete_path, true);
    bool success_dir_g = std::get<1>(dir_g); 
    
    if(!success_dir_g) {
        std::cout << "ERROR: Invalid path" << std::endl; 
        return 1;
    }

    uint16_t position_g = std::get<0>(dir_g);

    dir_entry d_entries[BLOCK_SIZE / sizeof(dir_entry)];

    disk.read(position_g, (uint8_t*)d_entries);

    auto split = split_file_name(complete_path);
    std::string name = std::get<0>(split);

    int access = stoi(accessrights);
    
    for(int i = 0; i < (BLOCK_SIZE / sizeof(dir_entry)); i++) {
        if(strcmp(d_entries[i].file_name, name.c_str()) == 0) {
            if(access == (0x04 | 0x02)) {
                d_entries[i].access_rights = (0x04 | 0x02); 

            } else if (access == (0x01)) {
                d_entries[i].access_rights = (0x01); 

            } else if (access == (0x02)) {
                d_entries[i].access_rights = (0x02); 

            } else if (access == (0x04)) {
                d_entries[i].access_rights = (0x04);

            } else if (access == (0x01 | 0x02)) {
                d_entries[i].access_rights = (0x01 | 0x02);
                
            } else if (access == (0x04 | 0x01)) {
                d_entries[i].access_rights = (0x04 | 0x01);
                
            } else if (access == (0x04 | 0x02 | 0x01)) {
                d_entries[i].access_rights = (0x04 | 0x02 | 0x01);

            } else {
                std::cout << "Invalid Permission: " << access << std::endl;
            }
        }
    }
    
    disk.write(position_g, (uint8_t*)d_entries);

    return 0;
}
