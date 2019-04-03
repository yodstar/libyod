#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "stdlog.h"
#include "rbtree.h"


#ifndef _YOD_RBTREE_DEBUG
#define _YOD_RBTREE_DEBUG 										0
#endif


/* yod_rbtree_v */
typedef struct _yod_rbtree_v
{
	uint64_t key;

	struct _yod_rbtree_v *left;
	struct _yod_rbtree_v *right;
	struct _yod_rbtree_v *parent;

	struct _yod_rbtree_v *next;
	struct _yod_rbtree_v *prev;

	byte color;
	void *value;
} yod_rbtree_v;


/* yod_rbtree_t */
struct _yod_rbtree_t
{
	pthread_mutex_t lock;

	ulong count;

	yod_rbtree_v *root;
	yod_rbtree_v *head;
	yod_rbtree_v *tail;
	yod_rbtree_v *curr;
	yod_rbtree_v *leaf;

	void (*vfree) (void * __ENV_CPARM);
};


#define yod_rbtree_is_red(n) 									((n) && (n)->color)
#define yod_rbtree_is_black(n) 									((n) && !(n)->color)

#define yod_rbtree_left_rotate(x, n) 							_yod_rbtree_left_rotate(x, n __ENV_CARGS)
#define yod_rbtree_right_rotate(x, n) 							_yod_rbtree_right_rotate(x, n __ENV_CARGS)


static void _yod_rbtree_left_rotate(yod_rbtree_t *self, yod_rbtree_v *node __ENV_CPARM);
static void _yod_rbtree_right_rotate(yod_rbtree_t *self, yod_rbtree_v *node __ENV_CPARM);


/** {{{ yod_rbtree_t *_yod_rbtree_new(void (*vfree) (void * __ENV_GPARM) __ENV_CPARM)
*/
yod_rbtree_t *_yod_rbtree_new(void (*vfree) (void * __ENV_CPARM) __ENV_CPARM)
{
	yod_rbtree_t *self = NULL;

	self = (yod_rbtree_t *) malloc(sizeof(yod_rbtree_t));
	if (!self) {
		YOD_STDLOG_ERROR("malloc failed");
		return NULL;
	}

	if (pthread_mutex_init(&self->lock, NULL) != 0) {
		free(self);

		YOD_STDLOG_ERROR("pthread_mutex_init failed");
		return NULL;
	}

	self->count = 0;

	self->root = NULL;
	self->head = NULL;
	self->tail = NULL;
	self->curr = NULL;

	self->leaf = (yod_rbtree_v *) malloc(sizeof(yod_rbtree_v));
	if (!self->leaf) {
		yod_rbtree_free(self);

		YOD_STDLOG_ERROR("malloc failed");
		return NULL;
	}
	self->leaf->key = 0;
	self->leaf->left = NULL;
	self->leaf->right = NULL;
	self->leaf->parent = NULL;
	self->leaf->next = NULL;
	self->leaf->prev = NULL;
	self->leaf->color = 0;
	self->leaf->value = NULL;

	self->root = self->leaf;

	self->vfree = vfree;

#if (_YOD_SYSTEM_DEBUG && _YOD_RBTREE_DEBUG)
	yod_stdlog_debug(NULL, "%s(%p): %p in %s:%d %s",
		__FUNCTION__, vfree, self, __ENV_TRACE);
#else
	__ENV_VOID
#endif

	return self;
}
/* }}} */


/** {{{ yod_rbtree_t *_yod_rbtree_new(__ENV_PARM)
*/
void _yod_rbtree_free(yod_rbtree_t *self __ENV_CPARM)
{
	yod_rbtree_v *node = NULL;

	if (!self) {
		return;
	}

#if (_YOD_SYSTEM_DEBUG && _YOD_RBTREE_DEBUG)
	yod_stdlog_debug(NULL, "%s(%p) in %s:%d %s",
		__FUNCTION__, self, __ENV_TRACE);
#else
	__ENV_VOID
#endif

	pthread_mutex_lock(&self->lock);
	for (node = self->head; node != NULL; node = self->curr) {
		self->curr = node->next;
		if (self->vfree) {
			self->vfree(node->value __ENV_CARGS);
		}
		free(node);
	}
	if (self->leaf) {
		free(self->leaf);
	}
	pthread_mutex_unlock(&self->lock);
	pthread_mutex_destroy(&self->lock);
	free(self);
}
/* }}} */


/** {{{ int _yod_rbtree_add(yod_rbtree_t *self, uint64_t key, void *value, int force __ENV_CPARM)
*/
int _yod_rbtree_add(yod_rbtree_t *self, uint64_t key, void *value, int force __ENV_CPARM)
{
	yod_rbtree_v *p, *node = NULL;
	int ret = 0;

	if (!self) {
		return (-1);
	}

	pthread_mutex_lock(&self->lock);

	if (force) {
		for (p = self->root; p != self->leaf;) {
			if (key == p->key) {
				node = p;
				break;
			}
			p = (key < p->key) ? p->left : p->right;
		}

		if (node) {
			if (self->vfree) {
				self->vfree(node->value __ENV_CARGS);
			}
			node->value = value;
			goto e_return;
		}
	}

	node = (yod_rbtree_v *) malloc(sizeof(yod_rbtree_v));
	if (!node) {
		ret = -1;

		YOD_STDLOG_ERROR("malloc failed");
		goto e_return;
	}

	node->key = key;

	node->left = self->leaf;
	node->right = self->leaf;
	node->parent = self->root;

	node->next = NULL;
	node->prev = NULL;

	node->color = 1; /* red */
	node->value = value;

	++ self->count;

	if (self->root == self->leaf) {
		node->color = 0; /* black */
		self->root = node;
		self->head = node;
		self->tail = node;
	}
	else {
		if (node->key < self->head->key) {
			self->head = node;
		} else if (node->key >= self->tail->key) {
			self->tail = node;
		}

		for (p = self->root; p != self->leaf;) {
			node->parent = p;
			p = (node->key < p->key) ? p->left : p->right;
		}

		p = node->parent;
		if (node->key < p->key) {
			p->left = node;
			node->next = p;
			node->prev = p->prev;
		} else {
			p->right = node;
			node->next = p->next;
			node->prev = p;
		}

		if (node->next) {
			node->next->prev = node;
		}
		if (node->prev) {
			node->prev->next = node;
		}

		/* re-balance tree */
		while (node != self->root && node->parent->color == 1) {
			if (node->parent == node->parent->parent->left) {
				p = node->parent->parent->right;
				if (p->color == 1) {
					node->parent->color = 0; /* black */
					p->color = 0; /* black */
					node->parent->parent->color = 1; /* red */
					node = node->parent->parent;
				} else {
					if (node == node->parent->right) {
						node = node->parent;
						yod_rbtree_left_rotate(self, node);
					}
					node->parent->color= 0;
					node->parent->parent->color = 1; /* red */
					yod_rbtree_right_rotate(self, node->parent->parent);
				}
			}
			else {
				p = node->parent->parent ? node->parent->parent->left : NULL;
				if (p->color == 1) {
					node->parent->color = 0; /* black */
					p->color = 0; /* black */
					node->parent->parent->color = 1;
					node = node->parent->parent;
				} else {
					if (node->parent && node == node->parent->left) {
						node = node->parent;
						yod_rbtree_right_rotate(self, node);
					}
					node->parent->color = 0;
					node->parent->parent->color = 1;
					yod_rbtree_left_rotate(self, node->parent->parent);
				}
			}
		}

		self->root->color = 0; /* black */
	}

e_return:

	pthread_mutex_unlock(&self->lock);

#if (_YOD_SYSTEM_DEBUG && _YOD_RBTREE_DEBUG)
	yod_stdlog_debug(NULL, "%s(%p, %lu, %p): %d in %s:%d %s",
		__FUNCTION__, self, (ulong) key, value, ret, __ENV_TRACE);
#else
	__ENV_VOID
#endif

	return ret;
}
/* }}} */


/** {{{ int _yod_rbtree_del(yod_rbtree_t *self, uint64_t key __ENV_CPARM)
*/
int _yod_rbtree_del(yod_rbtree_t *self, uint64_t key __ENV_CPARM)
{
	yod_rbtree_v *node = NULL;
	yod_rbtree_v *p, *t, *w;
	byte color = 0;
	int ret = 0;

	if (!self || !self->root) {
		return (-1);
	}

	pthread_mutex_lock(&self->lock);

	for (p = self->root; p != self->leaf;) {
		if (p->key == key) {
			node = p;
			break;
		}
		p = (key < p->key) ? p->left : p->right;
	}

	if (!node) {
		ret = -1;
		goto e_return;
	}

	-- self->count;

	{
		if (node == self->head) {
			self->head = node->next;
		}
		if (node == self->tail) {
			self->tail = node->prev;
		}
		if (node == self->curr) {
			self->curr = node->next;
		}
		if (node->next) {
			node->next->prev = node->prev;
		}
		if (node->prev) {
			node->prev->next = node->next;
		}
		if (self->vfree) {
			self->vfree(node->value __ENV_CARGS);
		}
	}

	if (node->left == self->leaf) {
		p = node->right;
		t = node;
	}
	else if (node->right == self->leaf) {
		p = node->left;
		t = node;
	} else {
		t = node->right;
		while (t->left != self->leaf) {
			t = t->left;
		}
		if (t->left != self->leaf) {
			p = t->left;
		} else {
			p = t->right;
		}
	}

	if (t == self->root) {
		self->root = p;
		p->color = 0; /* black */

		free(node);
		goto e_return;
	}

	color = t->color;

	if (t == t->parent->left) {
		t->parent->left = p;
	} else {
		t->parent->right = p;
	}

	if (t == node) {
		p->parent = t->parent;
	}
	else {
		if (t->parent == node) {
			p->parent = t;
		} else {
			p->parent = t->parent;
		}

		t->left = node->left;
		t->right = node->right;
		t->parent = node->parent;
		t->color = node->color;

		if (node == self->root) {
			self->root = t;
		}
		else {
			if (node == node->parent->left) {
				node->parent->left = t;
			} else {
				node->parent->right = t;
			}
		}

		if (t->left != self->leaf) {
			t->left->parent = t;
		}

		if (t->right != self->leaf) {
			t->right->parent = t;
		}
	}

	free(node);

	if (color) {
		goto e_return;
	}

	/* a delete fixup */
	while (p != self->root && !p->color) {
		if (p == p->parent->left) {
			w = p->parent->right;

			if (w->color) {
				w->color = 0; /* black */
				p->parent->color = 1; /* red */
				yod_rbtree_left_rotate(self, p->parent);
				w = p->parent->right;
			}

			if (!w->left->color && !w->right->color) {
				w->color = 1; /* red */
				p = p->parent;
			}
			else {
				if (!w->right->color) {
					w->left->color = 0; /* black */
					w->color = 1; /* red */
					yod_rbtree_right_rotate(self, w);
					w = p->parent->right;
				}
				w->color = p->parent->color;
				p->parent->color = 0; /* black */
				w->right->color = 0; /* black */
				yod_rbtree_left_rotate(self, p->parent);
				p = self->root;
			}

		} else {
			w = p->parent->left;

			if (w->color) {
				w->color = 0; /* black */
				p->parent->color = 1; /* red */
				yod_rbtree_right_rotate(self, p->parent);
				w = p->parent->left;
			}

			if (!w->left->color && !w->right->color) {
				w->color = 1; /* red */
				p = p->parent;
			}
			else {
				if (!w->left->color) {
					w->right->color = 0; /* black */
					w->color = 1; /* red */
					yod_rbtree_left_rotate(self, w);
					w = p->parent->left;
				}
				w->color = p->parent->color;
				p->parent->color = 0; /* black */
				w->left->color = 0; /* black */
				yod_rbtree_right_rotate(self, p->parent);
				p = self->root;
			}
		}
	}

	if (p != NULL) {
		p->color = 0; /* black */
	}

e_return:

	pthread_mutex_unlock(&self->lock);

#if (_YOD_SYSTEM_DEBUG && _YOD_RBTREE_DEBUG)
	yod_stdlog_debug(NULL, "%s(%p, %lu): %d in %s:%d %s",
		__FUNCTION__, self, (ulong) key, ret, __ENV_TRACE);
#else
	__ENV_VOID
#endif

	return ret;
}
/* }}} */


/** {{{ void *_yod_rbtree_find(yod_rbtree_t *self, uint64_t key __ENV_CPARM)
*/
void *_yod_rbtree_find(yod_rbtree_t *self, uint64_t key __ENV_CPARM)
{
	yod_rbtree_v *node = NULL;
	void *value = NULL;

	if (!self || !self->root) {
		return NULL;
	}

	pthread_mutex_lock(&self->lock);
	for (node = self->root; node != self->leaf;) {
		if (key == node->key) {
			value = node->value;
			break;
		}
		node = (key < node->key) ? node->left : node->right;
	}
	pthread_mutex_unlock(&self->lock);

#if (_YOD_SYSTEM_DEBUG && _YOD_RBTREE_DEBUG)
	yod_stdlog_debug(NULL, "%s(%p, %lu): %p in %s:%d %s",
		__FUNCTION__, self, (ulong) key, value, __ENV_TRACE);
#else
	__ENV_VOID
#endif

	return value;
}
/* }}} */


/** {{{ ulong _yod_rbtree_count(yod_rbtree_t *self __ENV_CPARM)
*/
ulong _yod_rbtree_count(yod_rbtree_t *self __ENV_CPARM)
{
	if (!self) {
		return 0;
	}

#if (_YOD_SYSTEM_DEBUG && (_YOD_RBTREE_DEBUG & 0x02))
	yod_stdlog_debug(NULL, "%s(%p): %lu in %s:%d %s",
		__FUNCTION__, self, self->count, __ENV_TRACE);
#else
	__ENV_VOID
#endif

	return self->count;
}
/* }}} */


/** {{{ void *_yod_rbtree_head(yod_rbtree_t *self, uint64_t *key __ENV_CPARM)
*/
void *_yod_rbtree_head(yod_rbtree_t *self, uint64_t *key __ENV_CPARM)
{
	void *value = NULL;
	
	if (!self || !self->root) {
		return NULL;
	}

	pthread_mutex_lock(&self->lock);
	if (self->head) {
		self->curr = self->head;
		if (key) {
			*key = self->curr->key;
		}
		value = self->curr->value;
	}
	pthread_mutex_unlock(&self->lock);

#if (_YOD_SYSTEM_DEBUG && _YOD_RBTREE_DEBUG)
	yod_stdlog_debug(NULL, "%s(%p, %lu): %p in %s:%d %s",
		__FUNCTION__, self, (ulong) (key ? *key : 0), value, __ENV_TRACE);
#else
	__ENV_VOID
#endif

	return value;
}
/* }}} */


/** {{{ void *_yod_rbtree_tail(yod_rbtree_t *self, uint64_t *key __ENV_CPARM)
*/
void *_yod_rbtree_tail(yod_rbtree_t *self, uint64_t *key __ENV_CPARM)
{
	void *value = NULL;
	
	if (!self || !self->root) {
		return NULL;
	}

	pthread_mutex_lock(&self->lock);
	if (self->tail) {
		self->curr = self->tail;
		if (key) {
			*key = self->curr->key;
		}
		value = self->curr->value;
	}
	pthread_mutex_unlock(&self->lock);

#if (_YOD_SYSTEM_DEBUG && _YOD_RBTREE_DEBUG)
	yod_stdlog_debug(NULL, "%s(%p, %lu): %p in %s:%d %s",
		__FUNCTION__, self, (ulong) (key ? *key : 0), value, __ENV_TRACE);
#else
	__ENV_VOID
#endif

	return value;
}
/* }}} */


/** {{{ void *_yod_rbtree_next(yod_rbtree_t *self, uint64_t *key __ENV_CPARM)
*/
void *_yod_rbtree_next(yod_rbtree_t *self, uint64_t *key __ENV_CPARM)
{
	void *value = NULL;
	
	if (!self || !self->root) {
		return NULL;
	}

	pthread_mutex_lock(&self->lock);
	if (self->curr && self->curr->next) {
		self->curr = self->curr->next;
		if (key) {
			*key = self->curr->key;
		}
		value = self->curr->value;
	}
	pthread_mutex_unlock(&self->lock);

#if (_YOD_SYSTEM_DEBUG && _YOD_RBTREE_DEBUG)
	yod_stdlog_debug(NULL, "%s(%p, %lu): %p in %s:%d %s",
		__FUNCTION__, self, (ulong) (key ? *key : 0), value, __ENV_TRACE);
#else
	__ENV_VOID
#endif

	return value;
}
/* }}} */


/** {{{ void *_yod_rbtree_prev(yod_rbtree_t *self, uint64_t *key __ENV_CPARM)
*/
void *_yod_rbtree_prev(yod_rbtree_t *self, uint64_t *key __ENV_CPARM)
{
	void *value = NULL;
	
	if (!self || !self->root) {
		return NULL;
	}

	pthread_mutex_lock(&self->lock);
	if (self->curr && self->curr->prev) {
		self->curr = self->curr->prev;
		if (key) {
			*key = self->curr->key;
		}
		value = self->curr->value;
	}
	pthread_mutex_unlock(&self->lock);

#if (_YOD_SYSTEM_DEBUG && _YOD_RBTREE_DEBUG)
	yod_stdlog_debug(NULL, "%s(%p, %lu): %p in %s:%d %s",
		__FUNCTION__, self, (ulong) (key ? *key : 0), value, __ENV_TRACE);
#else
	__ENV_VOID
#endif

	return value;
}
/* }}} */


/** {{{ static void _yod_rbtree_left_rotate(yod_rbtree_t *self, yod_rbtree_v *node __ENV_CPARM)
*/
static void _yod_rbtree_left_rotate(yod_rbtree_t *self, yod_rbtree_v *node __ENV_CPARM)
{
	yod_rbtree_v  *p = NULL;

	if (!self) {
		return;
	}

	p = node->right;
	node->right = p->left;
	if (p->left != self->leaf) {
		p->left->parent = node;
	}
	p->parent = node->parent;
	if (node == self->root) {
		self->root = p;
	} else if (node == node->parent->left) {
		node->parent->left = p;
	} else {
		node->parent->right = p;
	}
	p->left = node;
	node->parent = p;

#if (_YOD_SYSTEM_DEBUG && _YOD_RBTREE_DEBUG)
	yod_stdlog_debug(NULL, "%s(%p, %lu) in %s:%d %s",
		__FUNCTION__, self, (ulong) (node ? node->key : -1), __ENV_TRACE);
#else
	__ENV_VOID
#endif

}
/* }}} */


/** {{{ static void _yod_rbtree_right_rotate(yod_rbtree_t *self, yod_rbtree_v *node __ENV_CPARM)
*/
static void _yod_rbtree_right_rotate(yod_rbtree_t *self, yod_rbtree_v *node __ENV_CPARM)
{
	yod_rbtree_v  *p = NULL;

	if (!self) {
		return;
	}

	p = node->left;
	node->left = p->right;
	if (p->right != self->leaf) {
		p->right->parent = node;
	}
	p->parent = node->parent;
	if (node == self->root) {
		self->root = p;
	} else if (node == node->parent->right) {
		node->parent->right = p;
	} else {
		node->parent->left = p;
	}
	p->right = node;
	node->parent = p;

#if (_YOD_SYSTEM_DEBUG && _YOD_RBTREE_DEBUG)
	yod_stdlog_debug(NULL, "%s(%p, %lu) in %s:%d %s",
		__FUNCTION__, self, (ulong) (node ? node->key : -1), __ENV_TRACE);
#else
	__ENV_VOID
#endif
}
/* }}} */


/** {{{ static void _yod_rbtree_print(yod_rbtree_v *node, int n)
*/
static void _yod_rbtree_print(yod_rbtree_v *node, int n)
{
	int i = 0;

	if (!node) {
		return;
	}

	_yod_rbtree_print(node->left, n + 1);
	for (i = 0; i < n; ++i) {
		printf("\t\t");
	}
	if (node->color) {
		printf("[%ld]\n", (ulong) node->key);
	} else {
		printf("%ld\n", (ulong) node->key);
	}
	_yod_rbtree_print(node->right, n + 1);
}
/* }}} */


/** {{{ void yod_rbtree_print(yod_rbtree_t *self)
*/
void yod_rbtree_print(yod_rbtree_t *self)
{
	if (!self) {
		return;
	}

	_yod_rbtree_print(self->root, 0);
}
/* }}} */
