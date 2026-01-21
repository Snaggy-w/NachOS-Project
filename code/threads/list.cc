// list.cc
//
//      Routines to manage a singly-linked list of "things".
//
//      A "ListElement" is allocated for each item to be put on the
//      list; it is de-allocated when the item is removed. This means
//      we don't need to keep a "next" pointer in every object we
//      want to put on a list.
//
//      NOTE: Mutual exclusion must be provided by the caller.
//      If you want a synchronized list, you must use the routines
//      in synchlist.cc.
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation
// of liability and disclaimer of warranty provisions.

#include "list.h"
#include "copyright.h"

//----------------------------------------------------------------------
// ListElement::ListElement
//      Initialize a list element, so it can be added somewhere on a list.
//
//      "itemPtr" is the item to be put on the list.  It can be a pointer
//              to anything.
//      "sortKey" is the priority of the item, if any.
//----------------------------------------------------------------------

ListElement::ListElement(void *itemPtr, long long sortKey) {
    item = itemPtr;
    key = sortKey;
    next = NULL; // assume we'll put it at the end of the list
}

//----------------------------------------------------------------------
// List::List
//      Initialize a list, empty to start with.
//      Elements can now be added to the list.
//----------------------------------------------------------------------

List::List() { first = last = NULL; }

//----------------------------------------------------------------------
// List::~List
//      Prepare a list for deallocation.  If the list still contains any
//      ListElements, de-allocate them.  However, note that we do *not*
//      de-allocate the "items" on the list -- this module allocates
//      and de-allocates the ListElements to keep track of each item,
//      but a given item may be on multiple lists, so we can't
//      de-allocate them here.
//----------------------------------------------------------------------

List::~List() {
    while (Remove() != NULL)
        ; // delete all the list elements
}

//----------------------------------------------------------------------
// List::Append
//      Append an "item" to the end of the list.
//
//      Allocate a ListElement to keep track of the item.
//      If the list is empty, then this will be the only element.
//      Otherwise, put it at the end.
//
//      "item" is the thing to put on the list, it can be a pointer to
//              anything.
//----------------------------------------------------------------------

void List::Append(void *item) {
    ListElement *element = new ListElement(item, 0);

    if (IsEmpty()) { // list is empty
        first = element;
        last = element;
    } else { // else put it after last
        last->next = element;
        last = element;
    }
}

//----------------------------------------------------------------------
// List::Prepend
//      Put an "item" on the front of the list.
//
//      Allocate a ListElement to keep track of the item.
//      If the list is empty, then this will be the only element.
//      Otherwise, put it at the beginning.
//
//      "item" is the thing to put on the list, it can be a pointer to
//              anything.
//----------------------------------------------------------------------

void List::Prepend(void *item) {
    ListElement *element = new ListElement(item, 0);

    if (IsEmpty()) { // list is empty
        first = element;
        last = element;
    } else { // else put it before first
        element->next = first;
        first = element;
    }
}

//----------------------------------------------------------------------
// List::Remove
//      Remove the first "item" from the front of the list.
//
// Returns:
//      Pointer to removed item, NULL if nothing on the list.
//----------------------------------------------------------------------

void *List::Remove() {
    return SortedRemove(NULL); // Same as SortedRemove, but ignore the key
}

//----------------------------------------------------------------------
// List::Mapcar
//      Apply a function to each item on the list, by walking through
//      the list, one element at a time.
//
//      Unlike LISP, this mapcar does not return anything!
//
//      "func" is the procedure to apply to each element of the list.
//----------------------------------------------------------------------

void List::Mapcar(VoidFunctionPtr func) {
    for (ListElement *ptr = first; ptr != NULL; ptr = ptr->next) {
        DEBUG('l', "In mapcar, about to invoke %x(%x)\n", func, ptr->item);
        (*func)((int)ptr->item);
    }
}

//----------------------------------------------------------------------
// List::IsEmpty
//      Returns TRUE if the list is empty (has no items).
//----------------------------------------------------------------------

bool List::IsEmpty() {
    if (first == NULL)
        return TRUE;
    else
        return FALSE;
}

//----------------------------------------------------------------------
// List::SortedInsert
//      Insert an "item" into a list, so that the list elements are
//      sorted in increasing order by "sortKey".
//
//      Allocate a ListElement to keep track of the item.
//      If the list is empty, then this will be the only element.
//      Otherwise, walk through the list, one element at a time,
//      to find where the new item should be placed.
//
//      "item" is the thing to put on the list, it can be a pointer to
//              anything.
//      "sortKey" is the priority of the item.
//----------------------------------------------------------------------

void List::SortedInsert(void *item, long long sortKey) {
    ListElement *element = new ListElement(item, sortKey);
    ListElement *ptr; // keep track

    if (IsEmpty()) { // if list is empty, put
        first = element;
        last = element;
    } else if (sortKey < first->key) {
        // item goes on front of list
        element->next = first;
        first = element;
    } else { // look for first elt in list bigger than item
        for (ptr = first; ptr->next != NULL; ptr = ptr->next) {
            if (sortKey < ptr->next->key) {
                element->next = ptr->next;
                ptr->next = element;
                return;
            }
        }
        last->next = element; // item goes at end of list
        last = element;
    }
}

//----------------------------------------------------------------------
// List::SortedRemove
//      Remove the first "item" from the front of a sorted list.
//
// Returns:
//      Pointer to removed item, NULL if nothing on the list.
//      Sets *keyPtr to the priority value of the removed item
//      (this is needed by interrupt.cc, for instance).
//
//      "keyPtr" is a pointer to the location in which to store the
//              priority of the removed item.
//----------------------------------------------------------------------

void *List::SortedRemove(long long *keyPtr) {
    ListElement *element = first;
    void *thing;

    if (IsEmpty())
        return NULL;

    thing = first->item;
    if (first == last) { // list had one item, now has none
        first = NULL;
        last = NULL;
    } else {
        first = element->next;
    }
    if (keyPtr != NULL)
        *keyPtr = element->key;
    delete element;
    return thing;
}


void* List::SortedGet(long long key) {
    ListElement* curr = first;
    while (curr && curr->key < key) {
        curr = curr->next;
    }

    if (!curr || curr->key > key) {
        return nullptr;
    }

    return curr->item;
}

void *List::SortedRemoveByKey(long long key) {
    void* thing = nullptr;
    if (!first) {
        return thing;
    }

    if (first->key == key) {
        long long* keyPtr = nullptr;
        thing = SortedRemove(keyPtr);
    } else {
        ListElement* prev = first;
        while (prev && prev->next && prev->next->key < key) {
            prev = prev->next;
        }
        if (!prev->next || prev->next->key != key) {
            return thing;
        } 
        ListElement* victim = prev->next;
        if (victim == last) {
            last = prev;
        }
        prev->next = victim->next;
        thing = victim->item;

        delete victim;
    }

    return thing;
}


void List::InsertAt(void *item, int index){

	if(this->IsEmpty() || index <= 0){ // if list is empty or we give a negative index we prepend
		this->Prepend(item);
		return;
	}
	
	// create an element entry, same is done in prepend that's why we create it here 	
	ListElement *element = new ListElement(item,0);

	// walk through the linked list until index - 1 or the end if index is larger (append)
	ListElement *prev = first; 
	int i = 0;
	
	while(prev->next != NULL && i < index - 1){
		prev = prev->next;
		i++;
	}
	
	// insert the element in the list
	element->next = prev->next;
	prev->next = element;

	if(element->next == NULL) // update the last entry if the element is inserted at the end
		last = element; 
}

int List::Size() {
	int n = 0;
	for(ListElement *p = first; p != NULL; p = p->next) // walk through the list
		n++;
	return n;
}

//Returns the index of the element in the list if it exists.

int List::isExistElement(void* elt) {
    int index = 0;
    for (ListElement* p = first; p != NULL; p = p->next) {
        if (p->item == elt) {
            return index;
        }
        index++;
    }
    return -1; 
}

//Using a predefined comparator, return the index of the element which respects the comparator criteria.

int List::FindIf(ListComparator cmp, void* arg) {
    int index = 0;
    for (ListElement* p = first; p != NULL; p = p->next) {
        if (cmp(p->item, arg)) {
            return index;
        }
        index++;
    }
    return -1;
}

//Return the element specified by "index".

void *List::GetAt(int index){
	if(index < 0)
		return NULL;

	ListElement *p = first;
	int i = 0;

	while(p && i < index){
		p = p->next;
		i++;
	}
	// if we found the element we return the item, and if something is wrong we return NULL
	return p ? p->item : NULL; 
}


// completely remove an element without returning it
void List::DropAt(int index){ 
	if(this->IsEmpty() || index < 0){ // if list is empty or we give a negative index we do nothing
		return;
	}
	// walk through the linked list until index - 1 or the end if index is larger (append)
	ListElement *prev = first; 
	int i = 0;
	
	while(prev->next != NULL && i < index - 1){
		prev = prev->next;
		i++;
	}
	
	if(prev->next == NULL) // end of list
		return; // do nothing

	
	ListElement *target = prev->next;
	if(target == last) // update the last entry pointer
		last = prev;

	prev->next = target->next;
	delete target; // drop the list entry
	
}


void *List::RemoveAt(int index){
    if (IsEmpty() || index < 0) 
        return NULL;

    // remove first element
    if (index == 0) {
        return Remove();  // Remove() already deletes ListElement correctly and returns it so we reuse it
    }

    ListElement *prev = first;
    int i = 0;

    while (prev->next != NULL && i < index - 1) {
        prev = prev->next;
        i++;
    }

    // index out of range
    if (prev->next == NULL)
        return NULL;

    ListElement *target = prev->next;
    void *item = target->item;

    prev->next = target->next;
    if (target == last)
        last = prev;

    delete target;
    return item;
}

