/*--------------------------------------------------------------------
  Student Name: Jiehao Luo
  Student ID: 7852402
  Section: A01
--------------------------------------------------------------------*/
#include <stdio.h>
#include <unistd.h>
#include <limits.h>
#include <signal.h>
#include <time.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>
#include <stdbool.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <ctype.h>
#include "fat32.h"

#define _FILE_OFFSET_BITS 64

// Length of different parts of file name.
#define MAIN_PART_FILE_NAME_LENGTH 8
#define EXTENSION_LENGTH 3
#define MAX_LENGTH_OF_LONG_NAME 255

// Situations for the 1st bit of each entry.
#define DIRECTORY_ENTRY_IS_FREE 0xE5
#define LAST_EMPTY_DIRECTORY_ENTRY 0x00
#define KANJI 0x05

// Types for files
#define ATTR_READ_ONLY 0x01
#define ATTR_HIDDEN 0x02
#define ATTR_SYSTEM 0x04
#define ATTR_VOLUME_ID 0x08
#define ATTR_DIRECTORY 0x10
#define ATTR_ARCHIVE 0x20
#define ATTR_LONG_NAME 0x0F

int const ATTR_READ_ONLY_DIRECTORY = ATTR_READ_ONLY | ATTR_DIRECTORY;
int const ATTR_HIDDEN_DIRECTORY = ATTR_HIDDEN | ATTR_DIRECTORY;
int const ATTR_SYSTEM_DIRECTORY = ATTR_SYSTEM | ATTR_DIRECTORY;
int const ATTR_READ_ONLY_HIDDEN_DIRECTORY = ATTR_READ_ONLY | ATTR_HIDDEN | ATTR_DIRECTORY;
int const ATTR_READ_ONLY_SYSTEM_DIRECTORY = ATTR_READ_ONLY | ATTR_SYSTEM | ATTR_DIRECTORY;
int const ATTR_HIDDEN_SYSTEM_DIRECTORY = ATTR_HIDDEN | ATTR_SYSTEM | ATTR_DIRECTORY;
int const ATTR_READ_ONLY_HIDDEN_SYSTEM_DIRECTORY = ATTR_READ_ONLY | ATTR_HIDDEN | ATTR_SYSTEM | ATTR_DIRECTORY;

int const ATTR_READ_ONLY_ARCHIVE = ATTR_READ_ONLY | ATTR_ARCHIVE;
int const ATTR_HIDDEN_ARCHIVE = ATTR_HIDDEN | ATTR_ARCHIVE;
int const ATTR_SYSTEM_ARCHIVE = ATTR_SYSTEM | ATTR_ARCHIVE;
int const ATTR_READ_ONLY_HIDDEN_ARCHIVE = ATTR_READ_ONLY | ATTR_HIDDEN | ATTR_ARCHIVE;
int const ATTR_READ_ONLY_SYSTEM_ARCHIVE = ATTR_READ_ONLY | ATTR_SYSTEM | ATTR_ARCHIVE;
int const ATTR_HIDDEN_SYSTEM_ARCHIVE = ATTR_HIDDEN | ATTR_SYSTEM | ATTR_ARCHIVE;
int const ATTR_READ_ONLY_HIDDEN_SYSTEM_ARCHIVE = ATTR_READ_ONLY | ATTR_HIDDEN | ATTR_SYSTEM | ATTR_ARCHIVE;

#define BYTES_PER_DIRECTORY_ENTRY 32
#define BYTES_PER_FAT_ENTRY 4
#define THE_LAST_CLUSTER_OF_A_FILE 0xffffff8
#define VALUE_FOR_LAST_ENTRY_OF_LONG_NAME 1

// file descriptor for the image file 
int image;

// Some data from boot sector.
unsigned BPB_SecPerClus;
unsigned BPB_RsvdSecCnt;
unsigned BPB_NumFATs;
unsigned BPB_FATSz32;
unsigned BPB_BytesPerSec;

int entries_per_sector; 

// Two special file name. 
const unsigned char dot[11] = ".          ";
const unsigned char dotdot[11] = "..         ";

// Node object to hold a directory name.
typedef struct node_for_a_directory_struct
{
	char directory_name[MAX_LENGTH_OF_LONG_NAME + 1];
	struct node_for_a_directory_struct* next;
}node_for_a_directory;

// Linked list for directory names.
typedef struct list_of_directories_struct
{
	node_for_a_directory* head;
	node_for_a_directory* end;
}list_of_directories;

// Free a list_of_directories object.
void free_list(list_of_directories* list)
{
	if (list == NULL)
	{
		printf("Error: queue is NULL. \n");
		exit(EXIT_FAILURE);
	}

	node_for_a_directory* current = list->head;
	while (current != NULL)
	{
		node_for_a_directory* temp = current->next;
		free(current);
		current = temp;
	}

	list->head = NULL;
	list->end = NULL;
	free(list);
}

// Print a linked list
int print_list(list_of_directories* list)
{
	if (list == NULL)
	{
		printf("Error: queue is NULL. \n");
		exit(EXIT_FAILURE);
	}

	node_for_a_directory* current = list->head;

	// Number of dashes to print before "Directory".
	int count_of_dash = 0;
	while (current != NULL)
	{
		for (int i = 0; i < count_of_dash; i++)
		{
			printf("-");
		}
		printf("Directory: ");
		printf("%s\n", current->directory_name);
		count_of_dash++;
		current = current->next;
	}

	if (count_of_dash < 0)
	{
		printf("Error: count_of_dash is invalid. \n");
		exit(EXIT_FAILURE);
	}

	return count_of_dash;
}

// Push a node at the end of a linked list.
void push_at_end(list_of_directories* list, node_for_a_directory* node)
{
	if (list == NULL || node == NULL)
	{
		printf("Error: list or node is NULL. \n");
		exit(EXIT_FAILURE);
	}

	// If the queue is empty.
	if (list->head == NULL)
	{
		list->head = node;
	}
	if (list->end == NULL)
	{
		list->end = node;
	}
	// Add the new node at the end of the queue.
	else
	{
		list->end->next = node;
		list->end = node;
	}
}

// Copy a list to another.
void copy_list(list_of_directories* from, list_of_directories* to)
{
	if (from == NULL || to == NULL)
	{
		printf("Error: list 'from' or 'to' is NULL. \n");
		exit(EXIT_FAILURE);
	}

	node_for_a_directory* current = from->head;

	// Create a new node for each node of the old list, and push it to the new list.
	while (current != NULL)
	{
		node_for_a_directory* node = malloc(sizeof(node_for_a_directory));
		if (node == NULL)
		{
			printf("malloc failed. \n");
			exit(EXIT_FAILURE);
		}
		node->next = NULL;

		strcpy(node->directory_name, current->directory_name);
		push_at_end(to, node);

		current = current->next;
	}
}

// Check if a file's name is ".".
bool is_dot(unsigned char* filename)
{
	if (filename == NULL)
	{
		printf("Error: filename is NULL. \n");
		exit(EXIT_FAILURE);
	}

	bool result = true;

	// Compare every char in the filename with "dot" variable. 
	for (int i = 0; i < MAIN_PART_FILE_NAME_LENGTH + EXTENSION_LENGTH; i++)
	{
		if (filename[i] != dot[i])
		{
			result = false;
		}
	}

	return result;
}

// Check if a file's name is "..".
bool is_dotdot(unsigned char* filename)
{
	if (filename == NULL)
	{
		printf("Error: filename is NULL. \n");
		exit(EXIT_FAILURE);
	}

	bool result = true;

	// Compare every char in the filename with "dotdot" variable. 
	for (int i = 0; i < MAIN_PART_FILE_NAME_LENGTH + EXTENSION_LENGTH; i++)
	{
		if (filename[i] != dotdot[i])
		{
			result = false;
		}
	}

	return result;
}

// Trim whitespaces at the end of both the main part and extension part of a file name. 
char* trim_whitespaces(unsigned char* filename_in_11_length)
{
	if (filename_in_11_length == NULL)
	{
		printf("Error: filename_in_11_length is NULL. \n");
		exit(EXIT_FAILURE);
	}

	int whitespaces_in_main_part = 0;
	int whitespaces_in_extension = 0;

	// Count how many whitespaces are there at the end of main part. 
	int count = MAIN_PART_FILE_NAME_LENGTH - 1;
	bool hit_first_non_whitespace_char = false;
	while (!hit_first_non_whitespace_char && count >= 0)
	{
		if (filename_in_11_length[count] == ' ')
		{
			whitespaces_in_main_part++;
		}
		else
		{
			hit_first_non_whitespace_char = true;
		}
		count--;
	}

	// Count how many whitespaces are there at the end of extension. 
	count = MAIN_PART_FILE_NAME_LENGTH + EXTENSION_LENGTH - 1;
	hit_first_non_whitespace_char = false;
	while (!hit_first_non_whitespace_char && count >= MAIN_PART_FILE_NAME_LENGTH)
	{
		if (filename_in_11_length[count] == ' ')
		{
			whitespaces_in_extension++;
		}
		else
		{
			hit_first_non_whitespace_char = true;
		}
		count--;
	}

	// Length of the 2 parts without spaces. 
	int valid_length_main_part = MAIN_PART_FILE_NAME_LENGTH - whitespaces_in_main_part;
	int valid_length_extension = EXTENSION_LENGTH - whitespaces_in_extension;

	// The resulting file name's length is valid_length_main_part + valid_length_extension + one for '.' + one for '\0". 
	char* converted_filename = malloc(valid_length_main_part + valid_length_extension + 1 + 1);
	if (converted_filename == NULL)
	{
		printf("malloc failed. \n");
		exit(EXIT_FAILURE);
	}
	strcpy(converted_filename, "\0");

	// Copy the main part into the final result string. 
	for (int i = 0; i < valid_length_main_part; i++)
	{
		converted_filename[i] = filename_in_11_length[i];
	}

	// Add a "." to it if it has an extention. 
	if (valid_length_extension != 0)
	{
		converted_filename[valid_length_main_part] = '.';
	}

	// Copy the extension part into the final result string. 
	for (int i = 0; i < valid_length_extension; i++)
	{
		converted_filename[valid_length_main_part + 1 + i] = filename_in_11_length[MAIN_PART_FILE_NAME_LENGTH + i];
	}

	// Add "\0" to it.
	if (valid_length_extension != 0)
	{
		converted_filename[valid_length_main_part + valid_length_extension + 1] = '\0';
	}
	else
	{
		converted_filename[valid_length_main_part] = '\0';
	}

	return converted_filename;
}

// Recursively print a directory and its children. 
void print_a_directory(list_of_directories* parent_directories, unsigned starting_cluster, char* long_name)
{
	if (parent_directories == NULL)
	{
		printf("Error: parent_directories is NULL. \n");
		exit(EXIT_FAILURE);
	}

	if (starting_cluster < 2)
	{
		printf("Error: starting_cluster is invalid. \n");
		exit(EXIT_FAILURE);
	}

	// Calculate the offset in bytes of the FAT entry of this cluster.
	off_t offset_for_FAT_entry = BPB_RsvdSecCnt * BPB_BytesPerSec + starting_cluster * BYTES_PER_FAT_ENTRY;

	if (-1 == lseek(image, offset_for_FAT_entry, SEEK_SET))
	{
		printf("Error: lseek failed. \n");
		exit(EXIT_FAILURE);
	};

	// Get the content of the FAT entry, which is the next cluster. 
	unsigned next_cluster = THE_LAST_CLUSTER_OF_A_FILE;
	if (-1 == read(image, &next_cluster, sizeof(unsigned)))
	{
		printf("Error: read failed. \n");
		exit(EXIT_FAILURE);
	};

	// Mask out the first 4 bits. 
	unsigned mask = 0xfffffff;
	next_cluster = next_cluster & mask;

	// Calculate the offset of the first cluster of this directory.
	off_t offset_in_sectors = (starting_cluster - 2) * BPB_SecPerClus + BPB_RsvdSecCnt + BPB_NumFATs * BPB_FATSz32;
	off_t offset_in_bytes = offset_in_sectors * BPB_BytesPerSec;

	if (-1 == lseek(image, offset_in_bytes, SEEK_SET))
	{
		printf("Error: lseek failed. \n");
		exit(EXIT_FAILURE);
	};

	// Read the 1st entry of this directory. 
	fat32Dir directory;
	if (-1 == read(image, &directory, sizeof(fat32Dir)))
	{
		printf("Error: read failed. \n");
		exit(EXIT_FAILURE);
	};

	// Read each entry in this cluster one by one until hit the last empty one or the last one in this cluster. 
	int count_of_entries_checked = 1;
	while (directory.DIR_Name[0] != LAST_EMPTY_DIRECTORY_ENTRY && count_of_entries_checked <= entries_per_sector)
	{
		// Check one entry is not empty and it is of a valid type.
		if (directory.DIR_Name[0] != DIRECTORY_ENTRY_IS_FREE && !is_dot(directory.DIR_Name) && !is_dotdot(directory.DIR_Name))
		{
			if (directory.DIR_Attr == ATTR_LONG_NAME)
			{
				// Re-read this entry into fat32LDIR.
				fat32LDIR long_name_directory;
				if (-1 == lseek(image, offset_in_bytes, SEEK_SET))
				{
					printf("Error: lseek failed. \n");
					exit(EXIT_FAILURE);
				};
				if (-1 == read(image, &long_name_directory, sizeof(fat32LDIR)))
				{
					printf("Error: read failed. \n");
					exit(EXIT_FAILURE);
				};

				if (long_name == NULL)
				{
					long_name = malloc(MAX_LENGTH_OF_LONG_NAME + 1);
					if (long_name == NULL)
					{
						printf("Error: malloc failed. \n");
						exit(EXIT_FAILURE);
					}
					strcpy(long_name, "\0");
				}

				char* chars_in_this_entry = malloc(MAX_LENGTH_OF_LONG_NAME + 1);
				if (chars_in_this_entry == NULL)
				{
					printf("Error: malloc failed. \n");
					exit(EXIT_FAILURE);
				}
				strcpy(chars_in_this_entry, "\0");

				// If we have reach the last character of this entry. 
				bool hit_00 = false;
				int byte_already_read = 0;
				int bytes_to_read = 10;

				// Copy LDIR_Name1 into the final long name until we hit a byte of "00" which means there is no more to read. 
				while (!hit_00 && byte_already_read < bytes_to_read)
				{
					if (long_name_directory.LDIR_Name1[byte_already_read] == 0)
					{
						hit_00 = true;
					}
					else
					{
						strncat(chars_in_this_entry, &(long_name_directory.LDIR_Name1[byte_already_read]), 1);
						byte_already_read += 2;
					}
				}

				// Copy LDIR_Name2 into the final long name until we hit a byte of "00" which means there is no more to read. 
				byte_already_read = 0;
				bytes_to_read = 12;
				while (!hit_00 && byte_already_read < bytes_to_read)
				{
					if (long_name_directory.LDIR_Name2[byte_already_read] == 0)
					{
						hit_00 = true;
					}
					else
					{
						strncat(chars_in_this_entry, &(long_name_directory.LDIR_Name2[byte_already_read]), 1);
						byte_already_read += 2;
					}
				}

				// Copy LDIR_Name3 into the final long name until we hit a byte of "00" which means there is no more to read. 
				byte_already_read = 0;
				bytes_to_read = 4;
				while (!hit_00 && byte_already_read < bytes_to_read)
				{
					if (long_name_directory.LDIR_Name3[byte_already_read] == 0)
					{
						hit_00 = true;
					}
					else
					{
						strncat(chars_in_this_entry, &(long_name_directory.LDIR_Name3[byte_already_read]), 1);
						byte_already_read += 2;
					}
				}

				// Append chars_in_this_entry at the beginning of long_name.
				char* tmp = strdup(long_name);

				strcpy(long_name, chars_in_this_entry);
				strcat(long_name, tmp);

				free(tmp);
				free(chars_in_this_entry);
			}
			else if (directory.DIR_Attr == ATTR_READ_ONLY
				|| directory.DIR_Attr == ATTR_HIDDEN
				|| directory.DIR_Attr == ATTR_SYSTEM 
				|| directory.DIR_Attr == ATTR_DIRECTORY
				|| directory.DIR_Attr == ATTR_ARCHIVE
				|| directory.DIR_Attr == ATTR_READ_ONLY_DIRECTORY
				|| directory.DIR_Attr == ATTR_HIDDEN_DIRECTORY
				|| directory.DIR_Attr == ATTR_SYSTEM_DIRECTORY
				|| directory.DIR_Attr == ATTR_READ_ONLY_HIDDEN_DIRECTORY
				|| directory.DIR_Attr == ATTR_READ_ONLY_SYSTEM_DIRECTORY
				|| directory.DIR_Attr == ATTR_HIDDEN_SYSTEM_DIRECTORY
				|| directory.DIR_Attr == ATTR_READ_ONLY_HIDDEN_SYSTEM_DIRECTORY
				|| directory.DIR_Attr == ATTR_READ_ONLY_ARCHIVE
				|| directory.DIR_Attr == ATTR_HIDDEN_ARCHIVE
				|| directory.DIR_Attr == ATTR_SYSTEM_ARCHIVE
				|| directory.DIR_Attr == ATTR_READ_ONLY_HIDDEN_ARCHIVE
				|| directory.DIR_Attr == ATTR_READ_ONLY_SYSTEM_ARCHIVE
				|| directory.DIR_Attr == ATTR_HIDDEN_SYSTEM_ARCHIVE
				|| directory.DIR_Attr == ATTR_READ_ONLY_HIDDEN_SYSTEM_ARCHIVE)
			{
				// Replace the 1st bit if it is KANJI.
				if (directory.DIR_Name[0] == KANJI)
				{
					directory.DIR_Name[0] = DIRECTORY_ENTRY_IS_FREE;
				}

				// If the list for parent directories is not empty, print them and dashes associated. 
				int count_of_dash = 0;
				if (parent_directories->head != NULL)
				{
					count_of_dash = print_list(parent_directories);
				}
				for (int i = 0; i < count_of_dash; i++)
				{
					printf("-");
				}

				// If this is a Directory, print "Directory", otherwise print "file". 
				if (parent_directories->head != NULL)
				{
					if (directory.DIR_Attr == ATTR_DIRECTORY
						|| directory.DIR_Attr == ATTR_READ_ONLY_DIRECTORY
						|| directory.DIR_Attr == ATTR_HIDDEN_DIRECTORY
						|| directory.DIR_Attr == ATTR_SYSTEM_DIRECTORY
						|| directory.DIR_Attr == ATTR_READ_ONLY_HIDDEN_DIRECTORY
						|| directory.DIR_Attr == ATTR_READ_ONLY_SYSTEM_DIRECTORY
						|| directory.DIR_Attr == ATTR_HIDDEN_SYSTEM_DIRECTORY
						|| directory.DIR_Attr == ATTR_READ_ONLY_HIDDEN_SYSTEM_DIRECTORY
						)
					{
						printf("Directory: ");
					}
					else if (directory.DIR_Attr == ATTR_ARCHIVE 
						|| directory.DIR_Attr == ATTR_READ_ONLY_ARCHIVE
						|| directory.DIR_Attr == ATTR_HIDDEN_ARCHIVE
						|| directory.DIR_Attr == ATTR_SYSTEM_ARCHIVE
						|| directory.DIR_Attr == ATTR_READ_ONLY_HIDDEN_ARCHIVE
						|| directory.DIR_Attr == ATTR_READ_ONLY_SYSTEM_ARCHIVE
						|| directory.DIR_Attr == ATTR_HIDDEN_SYSTEM_ARCHIVE
						|| directory.DIR_Attr == ATTR_READ_ONLY_HIDDEN_SYSTEM_ARCHIVE)						
					{
						printf("File: ");
					}
				}

				// Print the file name.
				if (long_name != NULL)
				{
					printf("%s\n", long_name);
				}
				else
				{
					char* trtmmed_file_name = trim_whitespaces(directory.DIR_Name);
					printf("%s\n", trtmmed_file_name);
					free(trtmmed_file_name);
				}

				// If this entry is a directory, look into it. 
				if (directory.DIR_Attr == ATTR_DIRECTORY 
					|| directory.DIR_Attr == ATTR_READ_ONLY_DIRECTORY
					|| directory.DIR_Attr == ATTR_HIDDEN_DIRECTORY
					|| directory.DIR_Attr == ATTR_SYSTEM_DIRECTORY
					|| directory.DIR_Attr == ATTR_READ_ONLY_HIDDEN_DIRECTORY
					|| directory.DIR_Attr == ATTR_READ_ONLY_SYSTEM_DIRECTORY
					|| directory.DIR_Attr == ATTR_HIDDEN_SYSTEM_DIRECTORY
					|| directory.DIR_Attr == ATTR_READ_ONLY_HIDDEN_SYSTEM_DIRECTORY
					)
				{
					// Create a new node containing this directory's name. 
					node_for_a_directory* node = malloc(sizeof(node_for_a_directory));
					if (node == NULL)
					{
						printf("malloc failed. \n");
						exit(EXIT_FAILURE);
					}
					node->next = NULL;

					// Copy directory name into the node.
					if (long_name != NULL)
					{
						strcpy(node->directory_name, long_name);
					}
					else
					{
						for (int i = 0; i < MAIN_PART_FILE_NAME_LENGTH; i++)
						{
							node->directory_name[i] = directory.DIR_Name[i];
						}
						node->directory_name[MAIN_PART_FILE_NAME_LENGTH] = '\0';
					}

					// Copy a new list for the use of the next level of recursion. 
					list_of_directories* new_copy_of_list = malloc(sizeof(list_of_directories));
					if (new_copy_of_list == NULL)
					{
						printf("malloc failed. \n");
						exit(EXIT_FAILURE);
					}
					new_copy_of_list->head = NULL;
					new_copy_of_list->end = NULL;

					copy_list(parent_directories, new_copy_of_list);
					push_at_end(new_copy_of_list, node);

					// Recursively open the sub-directory. 
					uint32_t starting_cluster = (directory.DIR_FstClusHI << 16) | directory.DIR_FstClusLO;
					print_a_directory(new_copy_of_list, starting_cluster, NULL);
				}

				if (long_name != NULL)
				{
					free(long_name);
					long_name = NULL;
				}
			}
			else
			{
				free(long_name);
				long_name = NULL;
			}
		}
		
		// Read the next entry of this cluster.
		offset_in_bytes += sizeof(fat32Dir);
		if (-1 == lseek(image, offset_in_bytes, SEEK_SET))
		{
			printf("Error: lseek failed. \n");
			exit(EXIT_FAILURE);
		};
		if (-1 == read(image, &directory, sizeof(fat32Dir)))
		{
			printf("Error: read failed. \n");
			exit(EXIT_FAILURE);
		};
		count_of_entries_checked++;
	}// while

	// Open the next cluster of this directory.
	if (next_cluster < THE_LAST_CLUSTER_OF_A_FILE)
	{
		print_a_directory(parent_directories, next_cluster, long_name);
	}
	// If this the last cluster, do cleaning. 
	else
	{
		free_list(parent_directories);
	}
}

int main(int argc, char** argv)
{
	// Check if the user has entered the number representing the scheduling algorithm.
	if (!(argc == 3 || argc == 4))
	{
		fprintf(stderr, "Please enter the correct arguments. \n");
		exit(EXIT_FAILURE);
	}

	// Check if the command line argument is correct.
	if (argc == 3)
	{
		if (strcmp(argv[2], "info") != 0 && strcmp(argv[2], "list") != 0)
		{
			fprintf(stderr, "The command is incorrect. \n");
			exit(EXIT_FAILURE);
		}
	}
	if (argc == 4)
	{
		if (strcmp(argv[2], "get") != 0)
		{
			fprintf(stderr, "The command is incorrect. \n");
			exit(EXIT_FAILURE);
		}
	}

	char* filename = argv[1];
	char* command = argv[2];

    image = open(filename, O_RDONLY);
    if (image == -1)
    {
        printf("Unable to open file! \n");
        exit(EXIT_FAILURE);
    }

	// Read boot block.
	fat32BS boot_block;
	if (-1 == read(image, &boot_block, sizeof(fat32BS)))
	{
		printf("Error: read failed. \n");
		exit(EXIT_FAILURE);
	};
	// Check signatures in boot block. 
	if (!(boot_block.BS_SigA == 0x55 && boot_block.BS_SigB == 0xaa))
	{
		printf("Signature of boot block is incorrect! \n");
		exit(EXIT_FAILURE);
	}

	BPB_SecPerClus = boot_block.BPB_SecPerClus;
	BPB_RsvdSecCnt = boot_block.BPB_RsvdSecCnt;
	BPB_NumFATs = boot_block.BPB_NumFATs;
	BPB_FATSz32 = boot_block.BPB_FATSz32;
	BPB_BytesPerSec = boot_block.BPB_BytesPerSec;
	entries_per_sector = BPB_BytesPerSec / BYTES_PER_DIRECTORY_ENTRY;

	// Read FSInfo.
	FSInfo fs_info;
	if (-1 == lseek(image, boot_block.BPB_BytesPerSec, SEEK_SET))
	{
		printf("Error: lseek failed. \n");
		exit(EXIT_FAILURE);
	};
	if (-1 == read(image, &fs_info, sizeof(FSInfo)))
	{
		printf("Error: read failed. \n");
		exit(EXIT_FAILURE);
	};
	// Check signatures in FSInfo. 
	if (!(fs_info.FSI_LeadSig == 0x41615252 && fs_info.FSI_TrailSig == 0xAA550000))
	{
		printf("Signature of FSinfo is incorrect! \n");
		exit(EXIT_FAILURE);
	}

	// If the command is "info". 
	if (strcmp(command, "info") == 0)
	{
		printf("Drive name: %s\n", boot_block.BS_OEMName);

		// Calculate free space. 
		float free_bytes_in_KB = fs_info.FSI_Free_Count * boot_block.BPB_SecPerClus * boot_block.BPB_BytesPerSec / 1024.0f;
		printf("Free space on the drive in kB: %f\n", free_bytes_in_KB);

		// calculate usable storage. 
		int total_space = boot_block.BPB_TotSec32 * boot_block.BPB_BytesPerSec;
		int space_taken_by_FATs = boot_block.BPB_NumFATs * boot_block.BPB_BytesPerSec * boot_block.BPB_FATSz32;
		int usable_bytes = total_space - boot_block.BPB_RsvdSecCnt * boot_block.BPB_BytesPerSec - space_taken_by_FATs;
		printf("The amount of usable storage on the drive in bytes: %d\n", usable_bytes);

		printf("The cluster size in number of sectors: %d\n", boot_block.BPB_SecPerClus);
		printf("The cluster size in bytes: %d\n", boot_block.BPB_SecPerClus * boot_block.BPB_BytesPerSec);
	}
	// If the command is "list". 
	else if (strcmp(command, "list") == 0)
	{
		// Create a new list and open the root directory. 
		list_of_directories* list = malloc(sizeof(list_of_directories));
		if (list == NULL)
		{
			printf("malloc failed. \n");
			exit(EXIT_FAILURE);
		}
		list->head = NULL;
		list->end = NULL;

		print_a_directory(list, boot_block.BPB_RootClus, NULL);
	}
	// If the command is "get". 
	else if (strcmp(command, "get") == 0)
	{
		char* path = argv[3];
		char* token = strtok(path, "/");
		char* converted_filename = NULL;
		char extension[4];

		// Start from the root cluster. 
		unsigned cluster_being_checked = boot_block.BPB_RootClus;
		unsigned size_of_file_to_be_printed = 0;

		// Find the file. 
		while (token != NULL) 
		{
			converted_filename = malloc(MAX_LENGTH_OF_LONG_NAME + 1);
			if (converted_filename == NULL)
			{
				printf("malloc failed. \n");
				exit(EXIT_FAILURE);
			}
			strcpy(converted_filename, token);

			// Convert "token" to uppercase. 
			for (unsigned long i = 0; i < strlen(token); i++)
			{
				token[i] = toupper(token[i]);
			}

			// If we have found the file or directory specified by the user.
			bool found = false;
			// Do we need to check the next cluster of this directory.
			bool check_next_cluster_of_this_dir = true;
			char* long_name = NULL;

			while (check_next_cluster_of_this_dir)
			{
				// Calculate the offset of the cluster that currently being checked.
				off_t offset_in_sectors = (cluster_being_checked - 2) * BPB_SecPerClus + BPB_RsvdSecCnt + BPB_NumFATs * BPB_FATSz32;
				off_t offset_in_bytes = offset_in_sectors * BPB_BytesPerSec;
				if (-1 == lseek(image, offset_in_bytes, SEEK_SET))
				{
					printf("Error: lseek failed. \n");
					exit(EXIT_FAILURE);
				};

				// Read the 1st directory from that cluster. 
				fat32Dir directory;
				if (-1 == read(image, &directory, sizeof(fat32Dir)))
				{
					printf("Error: read failed. \n");
					exit(EXIT_FAILURE);
				};

				int count_of_entries_checked = 1;

				// While loop to check all direcories in this cluster. 
				while (!found && directory.DIR_Name[0] != LAST_EMPTY_DIRECTORY_ENTRY && count_of_entries_checked <= entries_per_sector)
				{
					if (directory.DIR_Name[0] != DIRECTORY_ENTRY_IS_FREE && !is_dot(directory.DIR_Name) && !is_dotdot(directory.DIR_Name))
					{
						if (directory.DIR_Attr == ATTR_READ_ONLY
							|| directory.DIR_Attr == ATTR_HIDDEN
							|| directory.DIR_Attr == ATTR_SYSTEM
							|| directory.DIR_Attr == ATTR_DIRECTORY
							|| directory.DIR_Attr == ATTR_ARCHIVE
							|| directory.DIR_Attr == ATTR_READ_ONLY_DIRECTORY
							|| directory.DIR_Attr == ATTR_HIDDEN_DIRECTORY
							|| directory.DIR_Attr == ATTR_SYSTEM_DIRECTORY
							|| directory.DIR_Attr == ATTR_READ_ONLY_HIDDEN_DIRECTORY
							|| directory.DIR_Attr == ATTR_READ_ONLY_SYSTEM_DIRECTORY
							|| directory.DIR_Attr == ATTR_HIDDEN_SYSTEM_DIRECTORY
							|| directory.DIR_Attr == ATTR_READ_ONLY_HIDDEN_SYSTEM_DIRECTORY
							|| directory.DIR_Attr == ATTR_READ_ONLY_ARCHIVE
							|| directory.DIR_Attr == ATTR_HIDDEN_ARCHIVE
							|| directory.DIR_Attr == ATTR_SYSTEM_ARCHIVE
							|| directory.DIR_Attr == ATTR_READ_ONLY_HIDDEN_ARCHIVE
							|| directory.DIR_Attr == ATTR_READ_ONLY_SYSTEM_ARCHIVE
							|| directory.DIR_Attr == ATTR_HIDDEN_SYSTEM_ARCHIVE
							|| directory.DIR_Attr == ATTR_READ_ONLY_HIDDEN_SYSTEM_ARCHIVE)
						{
							if (long_name != NULL)
							{
								// Convert "long_name" to uppercase. 
								for (unsigned long i = 0; i < strlen(long_name); i++)
								{
									long_name[i] = toupper(long_name[i]);
								}

								if (strcmp(token, long_name) == 0)
								{
									found = true;
									check_next_cluster_of_this_dir = false;
									cluster_being_checked = (directory.DIR_FstClusHI << 16) | directory.DIR_FstClusLO;
									size_of_file_to_be_printed = directory.DIR_FileSize;
									extension[0] = directory.DIR_Name[8];
									extension[1] = directory.DIR_Name[9];
									extension[2] = directory.DIR_Name[10];
								}
								free(long_name); 
								long_name = NULL;
							}
							else
							{
								char* trimmed_filename = trim_whitespaces(directory.DIR_Name);
								// If this file's name is identical with token, we have found our goal, therefore need to check next layer if there is.
								if (strcmp(token, trimmed_filename) == 0)
								{
									found = true;
									check_next_cluster_of_this_dir = false;
									cluster_being_checked = (directory.DIR_FstClusHI << 16) | directory.DIR_FstClusLO;
									size_of_file_to_be_printed = directory.DIR_FileSize;
									extension[0] = directory.DIR_Name[8];
									extension[1] = directory.DIR_Name[9];
									extension[2] = directory.DIR_Name[10];
								}
								free(trimmed_filename);
							}
						}
						else if(directory.DIR_Attr == ATTR_LONG_NAME)
						{
							// Re-read the entry into fat32LDIR.
							fat32LDIR long_name_directory;
							if (-1 == lseek(image, offset_in_bytes, SEEK_SET))
							{
								printf("Error: lseek failed. \n");
								exit(EXIT_FAILURE);
							};
							if (-1 == read(image, &long_name_directory, sizeof(fat32LDIR)))
							{
								printf("Error: read failed. \n");
								exit(EXIT_FAILURE);
							};

							if (long_name == NULL)
							{
								long_name = malloc(MAX_LENGTH_OF_LONG_NAME + 1);
								if (long_name == NULL)
								{
									printf("Error: malloc failed. \n");
									exit(EXIT_FAILURE);
								}
								strcpy(long_name, "\0");
							}

							char* chars_in_this_entry = malloc(MAX_LENGTH_OF_LONG_NAME + 1);
							if (chars_in_this_entry == NULL)
							{
								printf("Error: malloc failed. \n");
								exit(EXIT_FAILURE);
							}
							strcpy(chars_in_this_entry, "\0");

							// If we have reach the last byte of the last name. 
							bool hit_00 = false;
							int byte_already_read = 0;
							int bytes_to_read = 10;

							// Copy LDIR_Name1 into the final long name until we hit a byte of "00" which means there is no more to read. 
							while (!hit_00 && byte_already_read < bytes_to_read)
							{
								if (long_name_directory.LDIR_Name1[byte_already_read] == 0)
								{
									hit_00 = true;
								}
								else
								{
									strncat(chars_in_this_entry, &(long_name_directory.LDIR_Name1[byte_already_read]), 1);
									byte_already_read += 2;
								}
							}

							// Copy LDIR_Name2 into the final long name until we hit a byte of "00" which means there is no more to read. 
							byte_already_read = 0;
							bytes_to_read = 12;
							while (!hit_00 && byte_already_read < bytes_to_read)
							{
								if (long_name_directory.LDIR_Name2[byte_already_read] == 0)
								{
									hit_00 = true;
								}
								else
								{
									strncat(chars_in_this_entry, &(long_name_directory.LDIR_Name2[byte_already_read]), 1);
									byte_already_read += 2;
								}
							}

							// Copy LDIR_Name3 into the final long name until we hit a byte of "00" which means there is no more to read. 
							byte_already_read = 0;
							bytes_to_read = 4;
							while (!hit_00 && byte_already_read < bytes_to_read)
							{
								if (long_name_directory.LDIR_Name3[byte_already_read] == 0)
								{
									hit_00 = true;
								}
								else
								{
									strncat(chars_in_this_entry, &(long_name_directory.LDIR_Name3[byte_already_read]), 1);
									byte_already_read += 2;
								}
							}

							// Append chars_in_this_entry at the beginning of long_name. 
							char* tmp = strdup(long_name);

							strcpy(long_name, chars_in_this_entry);
							strcat(long_name, tmp);

							free(tmp);
							free(chars_in_this_entry);
						}
						else
						{
							free(long_name);
							long_name = NULL;
						}
					}

					// Read the next entry of this cluster.
					offset_in_bytes += sizeof(fat32Dir);
					if (-1 == lseek(image, offset_in_bytes, SEEK_SET))
					{
						printf("Error: lseek failed. \n");
						exit(EXIT_FAILURE);
					};
					if (-1 == read(image, &directory, sizeof(fat32Dir)))
					{
						printf("Error: read failed. \n");
						exit(EXIT_FAILURE);
					};
					count_of_entries_checked++;
				}// while

				// Didn't find the directory in this cluster, check the next cluster of this direcotry. 
				if (!found)
				{
					// Get the address of the next cluster from FAT.
					off_t offset_for_FAT_entry = BPB_RsvdSecCnt * BPB_BytesPerSec + cluster_being_checked * BYTES_PER_FAT_ENTRY;
					if (-1 == lseek(image, offset_for_FAT_entry, SEEK_SET))
					{
						printf("Error: lseek failed. \n");
						exit(EXIT_FAILURE);
					};
					if (-1 == read(image, &cluster_being_checked, sizeof(unsigned)))
					{
						printf("Error: read failed. \n");
						exit(EXIT_FAILURE);
					};

					// Mask out the first 4 bits. 
					unsigned mask = 0xfffffff;
					cluster_being_checked = cluster_being_checked & mask;

					// If there is no more cluster of this directory to check, stop the while loop, 
					// and the code below will print an error showing "Path doesn't exist".
					if (cluster_being_checked >= THE_LAST_CLUSTER_OF_A_FILE)
					{
						check_next_cluster_of_this_dir = false;
					}
				}
			}//while

			if (!found)
			{
				printf("Error: Path doesn't exist. \n");
				exit(EXIT_FAILURE);
			}

			token = strtok(NULL, "/");
			free(long_name);
			long_name = NULL;
		}// while token

		// Only .txt file is printable.
		if (strcmp(extension, "TXT") != 0)
		{
			printf("Error: The file to be printed out must be in .txt format. \n");
			exit(EXIT_FAILURE);
		}

		// Print the file
		int new_file = open(converted_filename, O_RDWR | O_TRUNC | O_CREAT, 0777);
		if (new_file == -1)
		{
			printf("Unable to create file! \n");
			exit(EXIT_FAILURE);
		}

		unsigned bytes_per_cluster = BPB_SecPerClus * BPB_BytesPerSec;
		unsigned bytes_read_so_far = 0;
		bool check_next_cluster_of_this_file = true;

		// Check every cluster of this file.
		while (check_next_cluster_of_this_file)
		{
			// Calculate the offset of the 1st cluster of that file to be printed.
			off_t offset_in_sectors = (cluster_being_checked - 2) * BPB_SecPerClus + BPB_RsvdSecCnt + BPB_NumFATs * BPB_FATSz32;
			off_t offset_in_bytes = offset_in_sectors * BPB_BytesPerSec;
			if (-1 == lseek(image, offset_in_bytes, SEEK_SET))
			{
				printf("Error: lseek failed. \n");
				exit(EXIT_FAILURE);
			};

			unsigned remaining_chars = size_of_file_to_be_printed - bytes_read_so_far;

			// Choose the min of the number of remaining chars and the number of bytes of each cluster as the number of chars to read this cluster.
			unsigned chars_to_read = 0;
			if (remaining_chars < bytes_per_cluster)
			{
				chars_to_read = remaining_chars;
			}
			else
			{
				chars_to_read = bytes_per_cluster;
			}

			// Read and write chars one by one.
			for (unsigned i = 0; i < chars_to_read; i++)
			{
				char one_char;
				if (-1 == read(image, &one_char, sizeof(char)))
				{
					printf("Error: read failed. \n");
					exit(EXIT_FAILURE);
				};
				if (write(new_file, &one_char, 1) == -1)
				{
					printf("Error: write failed. \n");
					exit(EXIT_FAILURE);
				};
			}

			bytes_read_so_far += chars_to_read;

			// Get the address of the next cluster of this file from FAT.
			off_t offset_for_FAT_entry = BPB_RsvdSecCnt * BPB_BytesPerSec + cluster_being_checked * BYTES_PER_FAT_ENTRY;
			if (-1 == lseek(image, offset_for_FAT_entry, SEEK_SET))
			{
				printf("Error: lseek failed. \n");
				exit(EXIT_FAILURE);
			};
			if (-1 == read(image, &cluster_being_checked, sizeof(unsigned)))
			{
				printf("Error: read failed. \n");
				exit(EXIT_FAILURE);
			};

			// Mask out the first 4 bits. 
			unsigned mask = 0xfffffff;
			cluster_being_checked = cluster_being_checked & mask;

			// If there is no more cluster of this directory to check, we are done.
			if (cluster_being_checked >= THE_LAST_CLUSTER_OF_A_FILE)
			{
				check_next_cluster_of_this_file = false;
			}
		}// while
		free(converted_filename);
		close(new_file);
	}
	else
	{
		fprintf(stderr, "The command is incorrect. \n");
		exit(EXIT_FAILURE);
	}

    close(image);
	exit(EXIT_SUCCESS);
}
