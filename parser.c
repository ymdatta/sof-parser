#include <errno.h>
#include <math.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Maximum length of IPC type or command name. */
#define INPUT_MAX_LEN 512

/* Value was taken randomly. */
#define MAX_IPC_SIZE 256

/* Timeout in microseconds. */
#define IPC_TIMEOUT 300

/* If size is valid, macro is replaced with 1 else 0 */
#define IS_VALID_SIZE(size) ((size > MAX_IPC_SIZE | size < 0) ? (0) : (1))

char ipc_types[][INPUT_MAX_LEN] = {"SOF_IPC_COMP_SET_VALUE",
				   "SOF_IPC_COMP_GET_VALUE",
				   "SOF_IPC_COMP_GET_DATA",
				   "SOF_IPC_COMP_SET_DATA",
				   "SOF_IPC_COMP_NOTIFICATION", "NULL"};

char ipc_cmds[][INPUT_MAX_LEN] = {"SOF_CTRL_CMD_VOLUME",
				  "SOF_CTRL_CMD_ENUM",
				  "SOF_CTRL_CMD_SWITCH",
				  "SOF_CTRL_CMD_BINARY", "NULL"};
/* Structure of the IPC message. */
/*    ipc_type: character array which stores name of ipc_type */
/*    ipc_cmd: character array which stores name of ipc_cmd */
/*    ipc_size: stores the payload size (in bytes) */
struct ipc_msg {
	char ipc_type[INPUT_MAX_LEN];
	char ipc_cmd[INPUT_MAX_LEN];
	int ipc_size;
};

/* IPC Message Linkedlist structure. */
/*     msg: points to an IPC message */
/*     next: points to next node in the linked list. */
struct ipc_msg_list {
	struct ipc_msg *msg;
	struct ipc_msg_list *next;
};

/* This function pushes a new message at the head of the messages
   linked list */
int push_to_msg_list(struct ipc_msg_list **head, struct ipc_msg *msg) {

	struct ipc_msg_list *new_msg = malloc(sizeof(struct ipc_msg_list));

	if(new_msg == NULL) {
		perror("malloc");
		return -ENOMEM;
	}

	new_msg->msg = msg;
	new_msg->next = *head;

	*head = new_msg;
	return 0;
}

/* A Linkedlist helper function - Frees the linkedlist */
void free_msg_list(struct ipc_msg_list **head) {
	struct ipc_msg_list *current = *head, *temp;

	while(current != NULL) {
		temp = current -> next;
		free(current->msg);
		free(current);
		current = temp;
	}
}

/* This function simulates the working of dsp.
   Only valid messages are sent to dsp, i.e,
   fitering happens before this function.

   This function sleeps for IPC_TIMEOUT Microseconds
   and prints the IPC_CMD of the message it received.

   returns 0 on success
*/
int send_msg_to_dsp(struct ipc_msg *msg) {
	// Using nanosleep instead of usleep for POSIX compatiblility
	struct timespec tim1;
	tim1.tv_sec = 1;

	// Converting micro seconds to nano seconds.
	tim1.tv_nsec = IPC_TIMEOUT * pow(10L, 6);

	if (nanosleep(&tim1, NULL) < 0) {
		perror("Nanosleep");
		return -1;
	}

	if (msg == NULL)
		return -EINVAL;

	printf("IPC command received: %s\n", msg->ipc_cmd);

	return 0;
}


/* The function checks if the IPC_CMD given to it as argument */
/* is a valid one. Returns 1 if valid else 0 */
int is_valid_ipc_cmd(char *ipc_cmd) {

	char *last_str = "NULL";
	for(int i = 0; ; ++i) {
		if(!strcmp(ipc_cmds[i], last_str))
			break;
		if(!strcmp(ipc_cmds[i], ipc_cmd))
			return 1;
	}
	return 0;
}

/* The function checks if the IPC_TYPE given to it as argument */
/* is a valid one. Returns 1 if valid else 0 */
int is_valid_ipc_type(char *ipc_type) {
	char *last_str = "NULL";

	for(int i = 0; ; ++i) {
		if(!strcmp(ipc_types[i], last_str))
			break;
		if(!strcmp(ipc_types[i], ipc_type))
			return 1;
	}
	return 0;
}

/* This function does the filtering of the messages,
   and also keeps track of details like number of
   invalid messages, invalid messages, max payload size,
   min payload size.

   The IPC message linked list is taken as argument, it
   is iterated over, and each message is checked if it
   has valid IPC_TYPE, IPC_CMD and valid IPC_SIZE.

   Only, after it's validated, it's sent to the dsp
   for further action.

   Also, the max/min payload is showed only when there is
   atleast one correct message.
*/
int simulate_msgs_send(struct ipc_msg_list *msg_list) {

	int max_payload = 0, min_payload = MAX_IPC_SIZE;
	int valid_msgs = 0, invalid_msgs = 0;

	struct ipc_msg_list *current = msg_list;

	while(current != NULL) {
		int valid_type_flag = 0, valid_cmd_flag = 0, valid_ipc_size = 0;

		valid_type_flag = is_valid_ipc_type(current->msg->ipc_type);
		valid_cmd_flag = is_valid_ipc_cmd(current->msg->ipc_cmd);
		valid_ipc_size = IS_VALID_SIZE(current->msg->ipc_size);

		if(valid_type_flag && valid_cmd_flag && valid_ipc_size) {
			valid_msgs += 1;

			if (current->msg->ipc_size > max_payload)
				max_payload = current->msg->ipc_size;
			if (current->msg->ipc_size < min_payload)
				min_payload = current->msg->ipc_size;

			int ret = send_msg_to_dsp(current->msg);
			if (ret) {
				printf("Message sending failed for Message:\n");
				printf("%s %s %d\n", current->msg->ipc_type,
				       current->msg->ipc_cmd, current->msg->ipc_size);
			}
		} else {
			invalid_msgs += 1;
		}
		current = current->next;
	}

	printf("Report:\n");
	printf("Valid Messages: %d Invalid Messages: %d\n", valid_msgs, invalid_msgs);
	if (valid_msgs)
		printf("Max payload: %d Min payload: %d\n", max_payload, min_payload);
	else
		printf("No valid messages to measure payload\n");

	return 0;
}

/* A Linkedlist helper function - Prints the messages in the list */
void print_msg_list(struct ipc_msg_list *head) {
	if (head == NULL)
		return;

	while (head != NULL) {
		struct ipc_msg *msg = head->msg;
		head = head->next;
		printf("%s %s %d\n", msg->ipc_type, msg->ipc_cmd, msg->ipc_size);
	}
}

int main(int argc, char**argv) {

	if (argc != 2) {
		printf("Usage: ./parser datafile\n");
		return -1;
	}

	// Head of the linkedlist which stores all the IPC Messages
	struct ipc_msg_list *msg_list = NULL;
	FILE* fptr = fopen(argv[1], "r");

	if(fptr == NULL) {
		perror( argv[1]);
		return -EINVAL;
	}
	while(1) {
		struct ipc_msg *msg = malloc(sizeof(struct ipc_msg));

		if(msg == NULL) {
			perror("malloc");
			return -ENOMEM;
		}

	        int fscan_ret = fscanf(fptr, "%s %s %d", msg->ipc_type, msg->ipc_cmd,
				       &msg->ipc_size);

		if (fscan_ret == EOF) {
			// This branch is taken when EOF is reached.
			free(msg);
			break;
		} else if (fscan_ret < 3) {
			// If this branch is taken, it means the input data was invalid.
			printf("Error: Invalid Message Input\n");
			free(msg);
			free_msg_list(&msg_list);
			fclose(fptr);
			return -1;
		}

		// Pushing the IPC Message read from the file into the linkedlist.
		int ret = push_to_msg_list(&msg_list, msg);
		if (ret < 0) {
			free(msg);
			free_msg_list(&msg_list);
			fclose(fptr);			
			return ret;
		}
	}
	// Printing all the messages read from the file
	printf("Printing the messages read from the file:\n");
	print_msg_list(msg_list);

	printf("\nSimulating Messages:\n");
	simulate_msgs_send (msg_list);

	//Freeing the linkedlist
	free_msg_list(&msg_list);

	//closing the file descriptor
	fclose(fptr);

	return 0;
}
