#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <signal.h>

#include "shell-utils.h"

#define INPUT_BUFFER_SIZE 2048
#define NB_MAX_TOKENS 512

int main() {
	/* Une variable pour sotcker les caractères tapés au clavier */
	char line[INPUT_BUFFER_SIZE+1];

	/* Une variable qui pointera vers les données lues par fgets
	 * Sauf problème (erreur, plus de données, ...), elle pointera en
	 * fait vers la variable précédente (line) */
	char* data;

	// Booleans
	int bg = 0;
	int pid;

	/* Un tableau de chaines où les mots de l'utilisateur seront stockés
	 * indépendamment les uns des autres
	 * Note: un mot est une suite de lettres séparées par une ou plusieurs
	 * espaces des autres mots.  */
	char* tokens[NB_MAX_TOKENS+1];
	/* variables entières pour comptabiliser le nombre de token */
	int nb_tokens;

	while(1)
	{
		printf("command : ");
		// fflush(stdout);
		/* Récupération des données tapées au clavier */
		data=fgets(line, INPUT_BUFFER_SIZE, stdin);


		if (data==NULL) {
			/* Erreur ou fin de fichier : on quitte tout de suite */
			if (errno) {
				/* Une erreur: on affiche le message correspondant
				 * et on quitte en indiquant une erreur */
				perror("fgets");
				exit(1);
			} else {
				/* Sinon ça doit être une fin de fichier.
				 * On l'indique et on quitte normalement */
				fprintf(stderr, "EOF: exiting\n");
				exit(0);
			}
		}

		/* On vérifie que l'utilisateur n'a pas donné une ligne trop longue */
		if (strlen(data) == INPUT_BUFFER_SIZE-1) {
			fprintf(stderr, "Input line too long: exiting\n");
			exit(1);
		}

		/* On découpe la ligne en tokens (mots)
		 * Voir sa documentation dans shell-utils.h (avec les exemples) */
		nb_tokens=split_tokens(tokens, data, NB_MAX_TOKENS);

		/* S'il y a trop de tokens, on abandonne */
		if (nb_tokens==NB_MAX_TOKENS) {
			fprintf(stderr, "Too many tokens: exiting\n");
			exit(1);
		}

		/* S'il n'y a pas de token, c'est que l'utilisateur n'a pas donné de
		 * commande. Il n'y a rien à faire. On arrête tout. */
		if (nb_tokens<=0) {
			fprintf(stderr, "Cannot split tokens: exiting\n");
			exit(1);
		}

		/* Si l'utilisateur tape "exit" on sort du shell */
		if (strcmp(tokens[0],"exit") == 0)
		{
			exit(0);
		}

		/* Detecting background processes */
		if (trouve_esperluette(tokens, nb_tokens))
		{
			bg = 1; /* Background mode : ON */
			if (fork() == 0)
			{
				execvp(tokens[0], tokens);
				fprintf(stderr,"%s : command not found\n", tokens[0]);
				exit(1); 
			}
		}
		else
		{
			bg = 0; /* Background mode : OFF */
		}


		if ((pid = fork()) == 0)
		{
				/********************************/
				/*			Opérateur >			*/
				/********************************/

			char* file_out = trouve_redirection(tokens, ">");

			if (file_out != NULL) // S'il existe une redirection dans la cmd
			{
				int file_descriptor;
				// Ici on vérifie si l'appel à open() s'est bien passé
				if ((file_descriptor = open(file_out, O_CREAT|O_TRUNC|O_WRONLY, 0644)) < 0)
				{
					perror(file_out);	// erreur
					exit(1);
				}
				dup2(file_descriptor, 1);
				close(file_descriptor);
			}

				/********************************/
				/*			Opérateur <			*/
				/********************************/

			char* file_in = trouve_redirection(tokens, "<");

			if (file_in != NULL) // S'il existe une redirection dans la cmd
			{
				int file_descriptor;
				// Ici on vérifie si l'appel à open() s'est bien passé
				if ((file_descriptor = open(file_in, O_RDONLY, 0644)) < 0)
				{
					perror(file_out);	// erreur
					exit(1);
				}

				dup2(file_descriptor, 0);
				close(file_descriptor);
			}	


				/********************************/
				/*			Opérateur |			*/
				/********************************/

		char** reste_commande;

		if ((reste_commande = trouve_tube(tokens, "|")) != NULL)
		{
			char** tmp = tokens;

			int PID;
			int fd[2];
			int fd_in = 0;
			int last = 0;

			while (tmp != NULL)
			{
				if (pipe(fd) == -1)
				{
					perror("pipe error");
	    			exit(1);
				} 

				// Utilisé pour le débogage
				// printf("<%s>\n", *(tmp));

				/* Ici l'idée est de fabriquer une sorte de "chaine" de pipes */
				if ((PID = fork()) == -1)
		        {
		          perror("fork");
		          exit(1);
		        }
			    else if (PID == 0)
		        {
		          dup2(fd_in, 0); // On change l'entrée selon l'ancienne
		          if (!last)
		            dup2(fd[1], 1);
		          close(fd[0]);
		          execvp(tmp[0], tmp);
		          perror("execvp");
		          exit(1);
		        }
			    else
		        {
		          wait(NULL);
		          close(fd[1]);
		          fd_in = fd[0]; // On garde l'entrée pour la prochaine commande
		          tmp = reste_commande;
				  reste_commande = trouve_tube(tmp, "|");
				  if (reste_commande == NULL)
				  {
				  	last = 1;
				  }
		        }
			}
		exit(0);
		}

		// char** reste_commande;
		// int last = 0;
		// if ((reste_commande = trouve_tube(tokens, "|")) != NULL)
		// {
		// 	char** tmp = tokens;
		// 	while (tmp[0] != NULL)
		// 	{
		// 		printf("<%s>\n", *(tmp));

		// 		tmp = reste_commande;
		// 		reste_commande = trouve_tube(tmp, "|");
		// 		// printf("reste_commande = <%s>\n", *(reste_commande));
		// 		if (reste_commande == NULL)
		// 		{
		// 			printf("LAST\n");
		// 			last = 1;
		// 		}
		// 	}
		// }

		// 	if (reste_commande != NULL)
		// 	{



		// 	char** cmd = tokens;

		// 	while (cmd != NULL)
		// 	{
		// 	// ne rentre ici que lorsque l'opérateur | est présent dans la commande 

		// 	// 	switch (PID = fork())
		// 	// 	{
		// 	// 		case 0: /* fils */
		// 	// 			close(fd[0]); 		/* pas besoin de garder ce bout */
		// 	// 			dup2(fd[1], 1);	/* ce bout du tube devient la sortie standard */
		// 	// 			execvp(tokens[0], tokens);	/* run the command */
		// 	// 			perror(tokens[0]);	/* it failed! */

		// 	// 		default: /* parent does nothing */
		// 	// 			break;

		// 	// 		case -1:
		// 	// 			perror("fork");
		// 	// 			exit(1);
		// 	// 	}

		// 	// 	switch (PID = fork())
		// 	// 	{
		// 	// 	case 0:  /* child */
		// 	// 		close(fd[1]);		/* this process doesn't need the other end */
		// 	// 		dup2(fd[0], 0);	/* this end of the pipe becomes the standard input */
		// 	// 		execvp(reste_commande[0], reste_commande);	/* run the command */
		// 	// 		perror(reste_commande[0]);	/* it failed! */

		// 	// 	default: /* parent does nothing */
		// 	// 		break;

		// 	// 	case -1:
		// 	// 		perror("fork");
		// 	// 		exit(1);
		// 	// 	}
		// 	// wait(NULL);
		// 	// close(fd[1]);
		// 	// wait(NULL);
		// 	// exit(0);

	 //      pipe(fd); 

	 //      if ((PID = fork()) == -1)
	 //      { 
	 //          exit(EXIT_FAILURE);
	 //      }

	 //      else if (PID == 0)
	 //      { 
	 //          dup2(fd_in, 0); //change the input according to the old one
	 //          if (reste_commande != NULL)
	 //            dup2(fd[1], 1);
	 //          close(fd[0]);
	 //          execvp(reste_commande[0], reste_commande);
	 //          exit(EXIT_FAILURE);
	 //      }
	 //      else
	 //      { 
	 //          wait(NULL);
	 //          close(fd[1]);
	 //          fd_in = fd[0]; //save the input for the next command
	 //          //tokens = trouve_tube(reste_commande, "|");
	 //      }
		// }
		// exit(0);
		// }

			/* On exécute la commande donné par l'utilisateur.
			 * Son nom est dans le premier token (le premier mot tapé)
			 * ses arguments (éventuels) seront les tokens suivants */
			execvp(tokens[0], tokens);
			
			fprintf(stderr,"%s : command not found\n", tokens[0]);
			exit(1); 
			
			/* On ne devrait jamais arriver là */
			// perror("execvp");
			// exit(1);
		}
		else
		{
			if (!bg) // cat n.txt | sort
			{
				int status;
				if (waitpid(pid, &status, 0) < 0)
				{
					perror("waitpid error");
				}
			}
		}
	}
}

/********** Version 2 pipes **********/

			// 	switch (PID = fork()) {

			// 	case 0: /* child 1 */
			// 		close(fd[0]); 		/* this process don't need the other end */
			// 		dup2(fd[1], 1);	/* this end of the pipe becomes the standard output */
			// 		execvp(tokens[0], tokens);	/* run the command */
			// 		perror(tokens[0]);	/* it failed! */

			// 	default: /* parent does nothing */
			// 		break;

			// 	case -1:
			// 		perror("fork");
			// 		exit(1);
			// 	}
			// 	wait(NULL);

			// 	switch (PID = fork())
			// 	{

			// 		case 0: /* child 2 */
			// 			close(fd[1]);		/* this process doesn't need the other end */
			// 			dup2(fd[0], 0);	/* this end of the pipe becomes the standard input */
			// 			execvp(reste_commande[0], reste_commande);	/* run the command */
			// 			perror(reste_commande[0]);	/* it failed! */

			// 		default: /* parent does nothing */
			// 			break;

			// 		case -1:
			// 			perror("fork");
			// 			exit(1);
			// 	}
			// 	close(fd[0]);close(fd[1]);
			// 	wait(NULL);
			// 	exit(0);
			// }

			/**************************************************/
