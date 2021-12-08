#include <iostream>
#include <fstream>
#include <cstring>
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

std::list<uint16_t> find_empty_fat_block(int amount); 

// utils

std::pair<dir_entry, bool> find_file(std::string filepath) {
    std::fstream myfile("diskfile.bin", std::ios::out | std::ios::in  | std::ios::binary);
    // TODO Handle: can't open file

    dir_entry file;
    dir_entry d_entries[BLOCK_SIZE / sizeof(dir_entry)];
    
    myfile.seekp(0); // dunno if this is neccessary... we prob already start at the beginning
    myfile.read((char*)&d_entries, BLOCK_SIZE);

    bool found_file = false;

    for(int i = 0; i < (BLOCK_SIZE / sizeof(dir_entry)); i++) {
        if(strcmp(filepath.c_str(), d_entries[i].file_name) == 0) {
            file = d_entries[i];
            found_file = true;
            break;
        }
    }

    return std::make_pair(file, found_file);
}

std::pair<bool, std::list<uint16_t>> 
FS::find_empty_dir_block(std::string pathname, uint32_t size, int type, int nr_blocks) {
    dir_entry d_entries[BLOCK_SIZE / sizeof(dir_entry)];

    bool success = false;

    // TODO change this later if sub-dir is goal
    disk.read(0, (uint8_t*)d_entries);

    //TODO Handle pwd path (dirpath) dir1/dir2... etc.
    dir_entry new_dir;
    strcpy(new_dir.file_name, pathname.c_str());

    std::list<uint16_t> f_entries = find_empty_fat_block(static_cast<int>(nr_blocks));

    if(f_entries.size() == 0) {
        return std::make_pair(success, f_entries);
    }

    new_dir.first_blk = f_entries.front();

    new_dir.size = size; 
    new_dir.type = type;
    // TODO fix rights later, need new parameter
    new_dir.access_rights = 0x06;

    for(int i = 0; i < (BLOCK_SIZE / sizeof(dir_entry)); i++) {
        if(d_entries[i].first_blk == 0) {
            d_entries[i] = new_dir; 
            success = true;
            break;
        } 
    }

    // TODO change this later if sub-dir is goal
    disk.write(0, (uint8_t*)d_entries);

    return std::make_pair(success, f_entries);
}

std::list<uint16_t>
FS::find_empty_fat_block(int amount) {

    int16_t f_entries[BLOCK_SIZE/2];
    std::list<uint16_t> f_pointers;

    disk.read(1, (uint8_t*)f_entries);
    
    // We need to know the FAT BEFORE so we can link it to the new FAT!
    int16_t tmp_fat_before = 0;

    // TODO: Handle files that are larger than 1 disk block, return multiple FAT pointers?
    for (int i = 0; i < BLOCK_SIZE/2; i++) {
        //std::cout << "Fat entry: " << i << " With number: " << load_f_entries[i] << std::endl;
        if (f_entries[i] == 0) {

           // TODO maybe find a more effective solution?
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

    std::cout << "List size: " << f_pointers.size() << std::endl;

    if (f_pointers.size() != amount) {
        std::cout << "ERROR: FULL FAT" << std::endl;
    } else {  
        // Write FAT block 
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

    // seems to be working
    //for (int i = 0; i < BLOCK_SIZE/2; i++) {
    //    std::cout << "Fat entry: " << i << " With number: " << f_entries[i] << std::endl;
    //}
	  
    std::fstream myfile("diskfile.bin", std::ios::out | std::ios::in  | std::ios::binary);

    if(!myfile) {
    	std::cout << "Cannot open file!" << std::endl;
	    return 1;
    }

    // write to disk block 1, i.e, fat block.
    myfile.seekp(BLOCK_SIZE);
    myfile.write((char*)&f_entries, sizeof(int16_t) * (BLOCK_SIZE / 2));

    int16_t load_f_entries[BLOCK_SIZE/2];

    // Check that the format succeeded
    myfile.seekp(BLOCK_SIZE);
    myfile.read((char*)&load_f_entries, sizeof(int16_t) * (BLOCK_SIZE / 2));


    for (int i = 0; i < BLOCK_SIZE/2; i++) {
        std::cout << "Fat entry: " << i << " With number: " << load_f_entries[i] << std::endl;
    }
	  
    std::cout << "Size of struct: " << sizeof(int16_t) << std::endl;

    myfile.close();

    return 0;
}

// create <filepath> creates a new file on the disk, the data content is
// written on the following rows (ended with an empty row)
int
FS::create(std::string filepath)
{
    std::cout << "FS::create(" << filepath << ")\n";
    
    std::string data;
    getline(std::cin, data);

    std::cout << data.length() << std::endl;

    // TODO: Couple things we need to handle:
    // * If string size is larger than possibly (56 chars)
    // * Check that filename does not already exist
    // * Directory and file cant have same name
    // * Correct access type 'rw-x' or 'rwx' when the file / directory is created

    // Init new file
    dir_entry new_file;
    strcpy(new_file.file_name, filepath.c_str());

    uint32_t s_data = data.length();

    new_file.size = s_data; // Size is zero since we dont have any data? However, we probably should find a starting disk block.

    float l_data = data.length();
    float s_block = BLOCK_SIZE;

    float calc = (l_data / s_block);
    float nr_blocks = std::ceil(calc);
    
    std::list<uint16_t> f_entries = find_empty_fat_block(static_cast<int>(nr_blocks));

    new_file.first_blk = f_entries.front();
    new_file.type = 0; // file = 0, directory = 1
    new_file.access_rights = 0x06; // TODO : Check this later

    // Read root
    // TODO: maybe we dont need to read the file multiple times
    std::fstream myfile("diskfile.bin", std::ios::out | std::ios::in  | std::ios::binary);
    
    if(!myfile) {
    	std::cout << "Cannot open file!" << std::endl;
	    return 1;
    }

    dir_entry d_entries[BLOCK_SIZE / sizeof(dir_entry)];
    
    // Find empty slot in root dict

    myfile.seekp(0);
    myfile.read((char*)&d_entries, BLOCK_SIZE);
   
    int tmp_empty_block = -1;

    for(int i = 0; i < (BLOCK_SIZE / sizeof(dir_entry)); i++) {
        if(d_entries[i].first_blk == 0) {
            // We found empty dir block.
            tmp_empty_block = i;
            d_entries[i] = new_file; 
            break;
        }
    }
    
    std::cout << "Empty dir block: " << tmp_empty_block << std::endl;

    if(tmp_empty_block == -1) {
        std::cout << "Couldn't find empty dir block" << std::endl;
        return 1;
    }

    disk.write(0, (uint8_t*)d_entries);

    std::cout << "Filename block: " << new_file.file_name << std::endl;

    // Write to disk.
    for(std::list<uint16_t>::iterator it = f_entries.begin(); it != f_entries.end(); it++) {
        std::cout << "Number of PAT's" << std::endl;
        std::cout << *it << std::endl;
       
    } 

    std::string tmp = data;
    
    char char_array[4096];
    
    std::list<uint16_t>::iterator it = f_entries.begin();

    for(int i = 0; i < nr_blocks; i++) {
        // Write data
        if (tmp.length() < 4096) {
            std::strcpy(char_array, tmp.c_str());
            disk.write(*it, (uint8_t*)char_array);
        } else {
            // TODO Test if this works later
            std::string d_write = tmp.substr(i * BLOCK_SIZE, BLOCK_SIZE);
            std::strcpy(char_array, d_write.c_str());
            disk.write(*it, (uint8_t*)char_array);
            tmp.erase(i * BLOCK_SIZE, BLOCK_SIZE);
            std::advance(it, 1);
        }
    }

    myfile.close();
    return 0;
}


// cat <filepath> reads the content of a file and prints it on the screen
int
FS::cat(std::string filepath)
{
    std::cout << "FS::cat(" << filepath << ")\n";
    
    std::fstream myfile("diskfile.bin", std::ios::out | std::ios::in  | std::ios::binary);
    
    auto t = find_file(filepath);

    dir_entry file = std::get<0>(t);
    bool success = std::get<1>(t);
    
    if(!success) {
        std::cout << "ERROR: could find file " << filepath << std::endl;
        return 1;
    }
    
    uint16_t f_entries[BLOCK_SIZE / sizeof(uint16_t)];

    disk.read(1, (uint8_t*)f_entries);

    std::list<uint16_t> f_file_entries;
    int16_t tmp = file.first_blk;

    // Find PAT's
    while(tmp != -1) {
        f_file_entries.push_back(tmp);
        tmp = f_entries[tmp];
    }   

    char data[4096];

    for(std::list<uint16_t>::iterator it = f_file_entries.begin(); it != f_file_entries.end(); it++) {
        disk.read(*it, (uint8_t*)data);
        std::cout << data << std::endl;
    } 

    myfile.close();

    return 0;
}

// ls lists the content in the currect directory (files and sub-directories)
int
FS::ls()
{
   
    // TODO Do not print (..) directories
    // TODO Handle current directory, pwd, instead of just root dict
    dir_entry dir_entries[BLOCK_SIZE / sizeof(dir_entry)];
    
    std::fstream myfile("diskfile.bin", std::ios::out | std::ios::in  | std::ios::binary);

    myfile.seekp(0);
    myfile.read((char*)&dir_entries, BLOCK_SIZE);

    std::list<dir_entry> content;

    for(int i = 0; i < (BLOCK_SIZE / sizeof(dir_entry)); i++) {
        //std::cout << "Content: " << dir_entries[i].file_name << std::endl;
        //std::cout << "Size: " << dir_entries[i].size << std::endl;
        //std::cout << "Pat: " << dir_entries[i].first_blk << std::endl;

        if(dir_entries[i].first_blk != 0) {
            content.push_back(dir_entries[i]);
        }
    }
   
    std::cout << "name" << std::setw(10) << "type" << std::setw(10) << "size" << std::endl;
    for(std::list<dir_entry>::iterator it = content.begin(); it != content.end(); it++) {
        std::string type_name;

        if((*it).type ==0) {
            type_name = "file";
        } else {
            type_name = "dir";
        }

        std::cout << std::left << std::setw(10) << (*it).file_name;
        std::cout << std::setw(10) << type_name;
        std::cout << std::right << (*it).size << std::endl; 
       
    } 
    
    /*
    for(int i = 0; i < content.size(); i++) {
        std::cout << "Content: " << content[i].file_name << std::endl;
        std::cout << "Size: " << content[i].size << std::endl;
        std::cout << "Pat: " << content[i].first_blk << std::endl;
    }
    */

    std::cout << "FS::ls()\n";

    myfile.close();

    return 0;
}

// cp <sourcepath> <destpath> makes an exact copy of the file
// <sourcepath> to a new file <destpath>
int
FS::cp(std::string sourcepath, std::string destpath)
{
    std::cout << "FS::cp(" << sourcepath << "," << destpath << ")\n";

    // Find file to copy
    auto t = find_file(sourcepath);
    bool success = std::get<1>(t);

    if(!success) {
        std::cout << "ERROR: Could not find file! -> " << sourcepath << std::endl;
        return 1;
    }

    dir_entry file_to_cpy = std::get<0>(t);

    // OK, file found, now copy();
    // TODO How do we handle if we copy to same dir?
    // TODO What happens if a file with same filename already exist in other directory
    
    auto g = find_file(destpath);
    bool success_g = std::get<1>(g);

    if(success_g) {
        std::cout << "ERROR: A file with that name already exists " << destpath << std::endl;
        return 1;
    }

    bool empty_file = true;

    float l_data = file_to_cpy.size;
    float s_block = BLOCK_SIZE;

    float calc = (l_data / s_block);
    float nr_blocks = std::ceil(calc);

    auto d = find_empty_dir_block(destpath, file_to_cpy.size, file_to_cpy.type, nr_blocks);
    bool success_d = std::get<0>(d);
    
    if(!success_d) {
        std::cout << "ERROR: Could not find empty dir block" << std::endl;
        return 1;
    }

    std::list<uint16_t> f_new_entries = std::get<1>(d);

    if(f_new_entries.size() != nr_blocks) {
        std::cout << "Couldn't find empty fat blocks" << std::endl;
        return 1;
    }

    uint16_t f_entries[BLOCK_SIZE / sizeof(uint16_t)];

    disk.read(1, (uint8_t*)f_entries);

    std::list<uint16_t> f_old_entries;
    int16_t tmp = file_to_cpy.first_blk;

    // Find PAT's
    while(tmp != -1) {
        f_old_entries.push_back(tmp);
        tmp = f_entries[tmp];
    }   

    char data[4096];

    std::list<uint16_t>::iterator it_old = f_old_entries.begin();
    std::list<uint16_t>::iterator it_new = f_new_entries.begin();

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

    // First of all we need to find the to move.
    // TODO Now we only handle path in root folder. need to handle etc dir3/filename
    auto t = find_file(sourcepath);
    bool success_t = std::get<1>(t);

    if(!success_t) {
        std::cout << "ERROR: Could not find file! -> " << sourcepath << std::endl;
        return 1;
    }

    // We found the file that we need to move
    // Now lets check that the destpath exists
    // TODO Now we only handle path in root folder. need to handle etc dir3/filename

    // If check that there does not already exists a file with that filename
    auto g = find_file(destpath);
    bool success_g = std::get<1>(g);

    if(!success_g) {
        std::cout << "ERROR: There already exists a file with that name" << destpath << std::endl;
    }

    dir_entry move_file = std::get<0>(t);
    strcpy(move_file.file_name, destpath.c_str());
   
    // So now we have changed the name of the file.
    // TODO we need to handle if there is different katalogs
    

    return 0;
}

// rm <filepath> removes / deletes the file <filepath>
int
FS::rm(std::string filepath)
{
    std::cout << "FS::rm(" << filepath << ")\n";
    return 0;
}

// append <filepath1> <filepath2> appends the contents of file <filepath1> to
// the end of file <filepath2>. The file <filepath1> is unchanged.
int
FS::append(std::string filepath1, std::string filepath2)
{
    std::cout << "FS::append(" << filepath1 << "," << filepath2 << ")\n";

    // First of all we check that filepath1 exists 
    // TODO: Handle if file is in other katalogs
    auto t = find_file(filepath1);
    bool success_t = std::get<1>(t);
    dir_entry file_t = std::get<0>(t);

    if(!success_t) {
        std::cout << "ERROR: Could not find file! -> " << filepath1 << std::endl;
        return 1;
    }

    // Found filepath1
    // Check filepath2
    auto g = find_file(filepath2);
    bool success_g = std::get<1>(g);
    dir_entry file_g = std::get<0>(g);

    if(!success_g) {
        std::cout << "ERROR: Could not find file! -> " << filepath2 << std::endl;
        return 1;
    }
    
    // We need to find the last PAT in filepath2 and then change the 
    // EOF to the first PAT in filepath1
    // TODO Should the size for filepath 2 increase?
    std::fstream myfile("diskfile.bin", std::ios::out | std::ios::in  | std::ios::binary);

    uint16_t f_entries[BLOCK_SIZE / sizeof(uint16_t)];

    myfile.seekp(BLOCK_SIZE);
    myfile.read((char*)&f_entries, BLOCK_SIZE);

    int16_t tmp = f_entries[file_g.first_blk];
    bool end_pat = false;

    uint16_t pat_loc;

    if(tmp == -1) {
        end_pat = true;
        pat_loc = file_g.first_blk;
    } else {

        while(!end_pat) {
            pat_loc = tmp;
            tmp = f_entries[tmp];

            if(tmp == -1) {
                end_pat = true;
            }
        } 
    }

    f_entries[pat_loc] = file_t.first_blk;
    myfile.seekp(BLOCK_SIZE);
    myfile.write((char*)&f_entries, BLOCK_SIZE);

    myfile.close();

    return 0;
}

// mkdir <dirpath> creates a new sub-directory with the name <dirpath>
// in the current directory
int
FS::mkdir(std::string dirpath)
{
    std::cout << "FS::mkdir(" << dirpath << ")\n";

    auto t = find_empty_dir_block(dirpath, 0, 1, 1);
    bool success = std::get<0>(t);
    std::list<uint16_t> f_entries = std::get<1>(t); 

    if(!success) {
        std::cout << "Could not create directory: " << dirpath << std::endl;
        return 1;
    }

    // Create sub_dir
    dir_entry sub_dir;
    strcpy(sub_dir.file_name, "..");

    // TODO Only points on root, however this may not always be true since there may be other sub-directories.
    sub_dir.first_blk = 0;
    sub_dir.size = 0;   
    sub_dir.type = 1;
    sub_dir.access_rights =0x06;

    dir_entry d_entries[BLOCK_SIZE / sizeof(dir_entry)];
    d_entries[0] = sub_dir;

    // Now we need to write.

    return 0;
}

// cd <dirpath> changes the current (working) directory to the directory named <dirpath>
int
FS::cd(std::string dirpath)
{
    std::cout << "FS::cd(" << dirpath << ")\n";

    std::string delimiter = "/";
    size_t pos = 0;

    std::fstream myfile("diskfile.bin", std::ios::out | std::ios::in  | std::ios::binary);
    // TODO Handle: couldn't open file

    dir_entry d_entries[BLOCK_SIZE / sizeof(dir_entry)];
    uint16_t seek_position = 0;
    std::string token;

    // Find current position
    if (strcmp("", path_pwd.c_str()) != 0) {
        std::string tmp;
        tmp = dirpath;

        while((pos = tmp.find(delimiter)) != std::string::npos) {
            token = tmp.substr(0, pos);
            std::cout << token << std::endl;
            tmp.erase(0, pos + delimiter.length());

            myfile.seekp(seek_position);
            myfile.read((char*)&d_entries, BLOCK_SIZE);
            

            for(int i = 0; i < (BLOCK_SIZE / sizeof(dir_entry)); i++) {

            } 
        }
    }

    while((pos = dirpath.find(delimiter)) != std::string::npos) {
        token = dirpath.substr(0, pos);
        std::cout << token << std::endl;
        dirpath.erase(0, pos + delimiter.length());
        


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

    return 0;
}

// chmod <accessrights> <filepath> changes the access rights for the
// file <filepath> to <accessrights>.
int
FS::chmod(std::string accessrights, std::string filepath)
{
    std::cout << "FS::chmod(" << accessrights << "," << filepath << ")\n";
    return 0;
}
