#include<stdlib.h>
#include<stdio.h>

#include"history.h"
#include"commit.h"

/**
  * new_history - alloue, initialise et retourne un historique.
  *
  * @name: nom de l'historique
  */
struct history *new_history(char *name)
{
  /* TODO : Exercice 3 - Question 2 */
  struct history *hist = malloc(sizeof(struct history));
  hist->name = name;
  hist->commit_count = 0;
  struct commit *comm =  new_commit(0, 0, "DO NOT PRINT ME !!!");
  comm->next = NULL;
  comm->prev = NULL;
  hist->commit_list = comm;
	return hist;
}

/**
  * last_commit - retourne l'adresse du dernier commit de l'historique.
  * @h: pointeur vers l'historique
  */
struct commit *last_commit(struct history *h)
{
	/* TODO : Exercice 3 - Question 2 */
  struct commit *temp = h->commit_list;
  while(temp->next != NULL) {
    temp = temp->next;
  }
	return temp;
}

/**
  * display_history - affiche tout l'historique, i.e. l'ensemble des commits de
  *                   la liste
  *
  * @h: pointeur vers l'historique a afficher
  */
void display_history(struct history *h)
{
	/* TODO : Exercice 3 - Question 4 */
  struct commit *temp = h->commit_list;
  int i = 0;
  char *stab;
  printf("Historique de '%s' :\n", h->name);
  while(temp->next != NULL) {
    i++;
    temp = temp->next;
    stab = (is_unstable(&temp->version)) ? "(unstable)" : "(stable)  ";
    printf("%i: %2u.%lu %s '%s'\n", i, temp->version.major, temp->version.minor, stab, temp->comment);
  }
}

/**
  * infos - affiche le commit qui a pour numero de version <major>-<minor> ou
  *         'Not here !!!' s'il n'y a pas de commit correspondant.
  *
  * @major: major du commit affiche
  * @minor: minor du commit affiche
  */
void infos(struct history *h, int major, unsigned long minor)
{
	/* TODO : Exercice 3 - Question 2 */
  struct commit *temp = h->commit_list;
  char *res = "Pas trouve";
  while(temp->next != NULL) {
    temp = temp->next;
    if(temp->version.major == major && temp->version.minor == minor) {
      res = temp->comment;
      break;
    }
  }
  printf("%s", res);
}
