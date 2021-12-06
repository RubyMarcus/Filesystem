#include <iostream>
#include <fstream>
#include <cstring>
#include <iomanip>
#include <cmath>
#include <list>
#include "fs.h"

#define BLOCK_SIZE 4096

FS::FS()
{
    std::cout << "FS::FS()... Creating file system\n";
}

FS::~FS()
{

}

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

std::list<uint16_t> find_empty_fat_block(int amount) {
    // Read FAT and return an integer that represent an empty FAT entry
    std::fstream myfile("diskfile.bin", std::ios::out | std::ios::in  | std::ios::binary);
    // TODO Handle: can't open file

    //if(!myfile) {
    //	std::cout << "Cannot open file!" << std::endl;
	//    return 1;
    //}

    int16_t load_f_entries[BLOCK_SIZE/2];
    std::list<uint16_t> f_pointers;

    myfile.seekp(BLOCK_SIZE);
    myfile.read((char*)&load_f_entries, sizeof(int16_t) * (BLOCK_SIZE / 2));
    
    // We need to know the FAT BEFORE so we can link it to the new FAT!
    int16_t tmp_fat_before = 0;

    // TODO: Handle files that are larger than 1 disk block, return multiple FAT pointers?
    for (int i = 0; i < BLOCK_SIZE/2; i++) {
        //std::cout << "Fat entry: " << i << " With number: " << load_f_entries[i] << std::endl;
        if (load_f_entries[i] == 0) {

           // TODO maybe find a more effective solution?
           if(f_pointers.size() == 0) {
               // First fat block
               tmp_fat_before = i;
               f_pointers.push_back(i);
               load_f_entries[i] = -1;           
               
               if(f_pointers.size() == amount) {
                    // In the case we only need 1 block
                    break;
               }
               continue;
           } else if (f_pointers.size() != amount) {
               tmp_fat_before = i;
               f_pointers.push_back(i);
               load_f_entries[i] = -1;
               load_f_entries[tmp_fat_before] = i;
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
        // Write FAT block 
        myfile.seekp(BLOCK_SIZE);
        myfile.write((char*)&load_f_entries, BLOCK_SIZE);
    }

    myfile.close();
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
    
    // Couple things we need to handle:
    // * If string size is larger than possibly (56 chars)
    // * Check that filename does not already exist
    // * Directory and file cant have same name
    // * Correct access type 'rw-x' or 'rwx' when the file / directory is created

    // Init new file
    dir_entry new_file;
//    new_file.file_name = std::string(filepath);
    //new_file.file_name[0] = "h"; 
    strcpy(new_file.file_name, filepath.c_str());
    new_file.size = 0; // Size is zero since we dont have any data? However, we probably should find a starting disk block.
    
    float nr_blocks = 1;
    if(new_file.size != 0) {
        nr_blocks = std::ceil(new_file.size / BLOCK_SIZE);
    }
    
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

    myfile.seekp(0);
    myfile.write((char*)&new_file, BLOCK_SIZE);

    std::cout << "Filename block: " << new_file.file_name << std::endl;

    /*
    for(int i = 0; i < BLOCK_SIZE / sizeof(dir_entry); i++) {
        std::cout << "PAT BLOCK: " << d_entries[i].first_blk << std::endl;        
        std::cout << "Filename: " << d_entries[i].file_name << std::endl;
    }
    */

    // Write to disk.
    // TODO How do we gather the data
    

    /*
    std::cout << "Found empty fat: " << new_file.first_blk << std::endl;
    */

    myfile.close();
    return 0;
}


// cat <filepath> reads the content of a file and prints it on the screen
int
FS::cat(std::string filepath)
{
    std::cout << "FS::cat(" << filepath << ")\n";
    
    std::fstream myfile("diskfile.bin", std::ios::out | std::ios::in  | std::ios::binary);
    
    dir_entry dir_entries[BLOCK_SIZE / sizeof(dir_entry)];
    
    myfile.read((char*)&dir_entries, BLOCK_SIZE);

    dir_entry file_to_read;
    int found_file = 0;

    // Find file to read
    for(int i = 0; i < (BLOCK_SIZE / sizeof(dir_entry)); i++) {
        if(strcmp(filepath.c_str(), dir_entries[i].file_name) == 0) {
            file_to_read = dir_entries[i]; 
            found_file = 1;
            break;
        }        
    }

    if(found_file == 0) {
        std::cout << "Can´t read file: " << filepath << std::endl; 
        return 1;
    }

    // Read file contents
    // TODO make data capable to store more than 1 disk block.
    char data[BLOCK_SIZE];
    int16_t tmp_pat = file_to_read.first_blk; 

    while(tmp_pat != -1) {
        myfile.seekp(BLOCK_SIZE * tmp_pat);
        // TODO maybe only read file contents (use file_to_read.size to control)
        myfile.read((char*)&data, BLOCK_SIZE);

        myfile.seekp(tmp_pat);
        myfile.read((char*)&tmp_pat, sizeof(int16_t)); 
    }

    // TODO print data

    myfile.close();

    return 0;
}

// ls lists the content in the currect directory (files and sub-directories)
int
FS::ls()
{
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
   
    // 
    std::cout << "name" << std::setw(10) << "size" << std::endl;
    for(std::list<dir_entry>::iterator it = content.begin(); it != content.end(); it++) {
        std::cout << std::left << std::setw(10) << (*it).file_name;
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
    
    bool empty_file = true;
    double nr_disks = 1;
    
    // Create new file 
    dir_entry new_file;
    // new_file.file_name = file_to_cpy.file_name; // TODO Change name if filename already exist in directory
    strcpy(new_file.file_name, file_to_cpy.file_name);

    // Calculate how many disk blocks we need! TODO Can size ever be 0? If so, handle this?
    if(file_to_cpy.size != 0)
    {
        double nr_disks = std::ceil(file_to_cpy.size / BLOCK_SIZE);
        empty_file = false;
    }
    
    std::list<uint16_t> f_entries = find_empty_fat_block(static_cast<int>(f_entries));

    new_file.first_blk = f_entries.front();
    new_file.size = file_to_cpy.size;
    new_file.type = file_to_cpy.type;
    new_file.access_rights = file_to_cpy.access_rights;

    if(new_file.first_blk == -1) {
        std::cout << "Couldn't find empty fat block" << std::endl;
        return 1;
    }
    
    std::fstream myfile("diskfile.bin", std::ios::out | std::ios::in  | std::ios::binary);

    if (!empty_file) {
        // TODO: Start copy disk blocks 
        for(std::list<uint16_t>::iterator it = f_entries.begin(); it != f_entries.end(); it++) {
            myfile.seekp(it * BLOCK_SIZE);
            // Write data
        }
    }        
    
    myfile.close();
    return 0;
}

// mv <sourcepath> <destpath> renames the file <sourcepath> to the name <destpath>,
// or moves the file <sourcepath> to the directory <destpath> (if dest is a directory)
int
FS::mv(std::string sourcepath, std::string destpath)
{
    std::cout << "FS::mv(" << sourcepath << "," << destpath << ")\n";

    // First of all we need to find the to move.
    
    
    

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
    return 0;
}

// mkdir <dirpath> creates a new sub-directory with the name <dirpath>
// in the current directory
int
FS::mkdir(std::string dirpath)
{
    std::cout << "FS::mkdir(" << dirpath << ")\n";
    return 0;
}

// cd <dirpath> changes the current (working) directory to the directory named <dirpath>
int
FS::cd(std::string dirpath)
{
    std::cout << "FS::cd(" << dirpath << ")\n";
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
