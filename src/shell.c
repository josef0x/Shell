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
			{	/* On execute la commande dans le processus fils */
				execvp(tokens[0], tokens);
				fprintf(stderr,"%s : command not found\n", tokens[0]);
				exit(1); 
			}
		}
		else
		{	/* FLAG (booléan) bg sera utilisé après pour indiquer au père *
			 *          	s'il doit attendre ou pas                     */
			bg = 0; /* Background mode : OFF */
		}


		if ((pid = fork()) == 0)
		{
				/********************************/
				/*			Opérateur >			*/
				/********************************/

			char* file_out = trouve_redirection(tokens, ">");

			/* S'il existe une redirection dans la cmd */
			if (file_out != NULL)
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

			/* S'il existe une redirection dans la cmd */
			if (file_in != NULL)
			{
				int file_descriptor;
				/* Ici on vérifie si l'appel à open() s'est bien passé */
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
			char** cmd = tokens;

			int PID;
			int fd[2];
			int joint = 0;
			int last = 0;

			while (cmd != NULL)
			{
				if (pipe(fd) == -1)
				{
					perror("pipe error");
	    			exit(1);
				} 

				// Utilisé pour le débogage
				// printf("<%s>\n", *(cmd));

				/* Ici l'idée est de fabriquer une sorte de "chaine" de pipes */
				
				if ((PID = fork()) == -1)
		        { /* En cas d'erreur */
		          perror("fork");
		          exit(1);
		        }
			    else if (PID == 0) /* Fils */
		        {
		          dup2(joint, 0); /* On change l'entrée selon l'ancienne */
		          
		          if (!last) /* Si ce n'est pas la dernière commande de la chaine */
		            dup2(fd[1], 1);

		          close(fd[0]);
		          execvp(cmd[0], cmd);
		          perror("execvp");
		          exit(1);
		        }
			    else
		        {
		          wait(NULL);

		          close(fd[1]);

		          joint = fd[0]; /* On garde l'entrée pour la prochaine commande */
		          	
		          cmd = reste_commande;
				  reste_commande = trouve_tube(cmd, "|");

				  if (reste_commande == NULL)
				  {
				  	last = 1;
				  }
		        }
			}
		exit(0); /* On ne veut pas partir plus loin ! */
		}
			/* On exécute la commande donné par l'utilisateur.
			 * Son nom est dans le premier token (le premier mot tapé)
			 * ses arguments (éventuels) seront les tokens suivants */
			execvp(tokens[0], tokens);

			/* Gestion de l'erreur */
			fprintf(stderr,"%s : command not found\n", tokens[0]);
			exit(1); 
			
			/* On ne devrait jamais arriver là */
			// perror("execvp");
			// exit(1);
		}
		else
		{	/* Parent */
			if (!bg) /* S'il ne s'agit pas d'une commande en arrière-plan */
			{
				int status;
				/* On attend */
				if (waitpid(pid, &status, 0) < 0)
				{
					perror("waitpid error");
				}
			}
		}
	}
}