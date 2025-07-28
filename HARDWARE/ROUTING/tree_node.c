
#include "tree_node.h" 
#include "FreeRTOS.h"


/**
  * @brief  创建树节点，返回指向节点的指针
  * @param  addr：节点的地址
  * @retval NULL:创建失败;其他：创建成功
  */
TreeNode* createNode(NodeAddr addr) 
{
    TreeNode* newNode = (TreeNode*)pvPortMalloc(sizeof(TreeNode));
    if (newNode != NULL) 
    {
        newNode->addr = addr;
        newNode->firstChild = NULL;
        newNode->nextBrother = NULL;
        newNode->is_node_alive = 1; //默认创建时节点存活
    }
    return newNode;
}


/**
  * @brief  释放以某个节点为根的树的内存
  * @param  root：树的根节点
  * @retval None
  */
void freeTree(TreeNode* root) 
 {
    if (root != NULL) 
    {
        freeTree(root->firstChild);
        freeTree(root->nextBrother);
        vPortFree(root);
    }
}

/**
  * @brief  寻找某个节点的父节点
  * @param  t：寻找范围内的根节点；p：寻找父节点的节点；result：存放父节点地址的指针
  * @retval None
  */
void FindFather(TreeNode* t, TreeNode* p, TreeNode** result) {
  *result = NULL;

  if (t == NULL || p == NULL || p == t) {
      return;
  }

  TreeNode* q = t->firstChild;

  while (q != NULL) {
      if (q == p) {
          *result = t;
          return;
      }

      FindFather(q, p, result);

      if (*result != NULL) {
          return;
      }

      q = q->nextBrother;
  }
}

/**
  * @brief  删除并释放树节点及以其为根子树
  * @param  t：指向树的根节点的指针；p：指向要删除的子树的根节点的指针
  * @retval None
  */
void DelSubtree(TreeNode* t, TreeNode* p) {
    if (t == NULL || p == NULL) {
        return;
    }

    TreeNode* result = NULL;
    FindFather(t, p, &result);

    if (result == NULL) {
        return; // 未找到父亲节点
    }

    if (result->firstChild == p) {
        result->firstChild = p->nextBrother;
        freeTree(p);
        return;
    }

    TreeNode* q = result->firstChild;

    while (q != NULL && q->nextBrother != p) {
        q = q->nextBrother;
    }

    if (q != NULL) {
        q->nextBrother = p->nextBrother;
        freeTree(p);
    }
}

// 算法 FindTarget
// 返回对应节点指针

/**
  * @brief  根据地址寻找节点
  * @param  t：寻找范围内的根节点；addr：寻找的节点的地址；result：存放寻找的节点地址的指针
  * @retval None
  */
void FindTarget(TreeNode* t, NodeAddr addr, TreeNode** result) {
    *result = NULL;

    if (t == NULL) {
        return;
    }

    if (t->addr == addr) {
        *result = t;
        return;
    }

    TreeNode* p = t->firstChild;

    while (p != NULL) {
        FindTarget(p, addr, result);

        if (*result != NULL) {
            return;
        }

        p = p->nextBrother;
    }
}

/**
  * @brief  为某个节点添加子节点
  * @param  father：指向待添加子节点的节点指针；child：指向添加的子节点指针
  * @retval None
  */
void AddChild(TreeNode* father, TreeNode* child)
{
    TreeNode* p;
    if(father->firstChild == NULL)//如果父节点没有子节点
    {
        father->firstChild = child;  
        return;
    }
    p = father->firstChild;
    while (p->nextBrother)//退出循环时p指向最右边的子节点
    {
        p = p->nextBrother;
    }
    p->nextBrother = child;
}

/**
  * @brief  断开与父节点的联系,但不释放子节点内存
  * @param  rootnode：整棵树的根节点；child：指向待与其父节点断开的节点的指针
  * @retval None
  */
void Disconnect(TreeNode* rootnode, TreeNode* child)
{
        TreeNode* result = NULL;
        if (rootnode == NULL || child == NULL) {
                return;
        }
        FindFather(rootnode, child, &result);
        if (result == NULL) {
            return; // 未找到父亲节点
        }
    
        if (result->firstChild == child) {
            result->firstChild = child->nextBrother;
            child->nextBrother = NULL;
            return;
        }
        TreeNode* q = result->firstChild;
        while (q != NULL && q->nextBrother != child) {
            q = q->nextBrother;
        }
        if (q != NULL) {
            q->nextBrother = child->nextBrother;
            child->nextBrother = NULL;
        }    
}

/**
  * @brief  修改子节点,将子节点及其子树添加到新的父节点下，并断开与旧的父节点的联系
  * @param  rootnode：整棵树的根节点；newfather：新的父节点；child：指向待操作的节点的指针
  * @retval None
  */
void ChangeChild(TreeNode* rootnode, TreeNode* newfather, TreeNode* child)
{
    TreeNode* p;
    Disconnect(rootnode, child);

    if(newfather->firstChild == NULL)//如果父节点没有子节点
    {
        newfather->firstChild = child;
        return;
    }
    p = newfather->firstChild;
    while (p->nextBrother)//退出循环时p指向最右边的子节点
    {
        p = p->nextBrother;
    }
    p->nextBrother = child;
}

/**
  * @brief  遍历所有节点，打印整棵树的子节点，不包括根节点
  * @param  root：整棵树的根节点；
  * @retval None
  */
void TraverseTree(TreeNode* root) {
    if (root == NULL) {
        return;
    }

    TreeNode* p = root->firstChild;

    while (p != NULL) {
        TraverseTree(p);
        printf("Child Node ADDR: %llx, is alive? %x\r\n", p->addr, p->is_node_alive);
        p = p->nextBrother;
    }
}

/**
  * @brief  扫描死掉的子节点
  * @param  root_itera：整棵树的根节点,用于迭代；
  *         root_r：整棵树的根节点；
  * @retval None
  */
 TreeNode *wait_del[64]; //死亡的子节点指针列表
 uint8_t del_number = 0; //死亡的子节点个数
 void ScanDeadNode(TreeNode* root_itera, TreeNode* root_r) {
     if (root_itera == NULL) {
         return;
     }
 
     TreeNode* p = root_itera->firstChild;
     while (p != NULL) {
        ScanDeadNode(p, root_r);
        if(p->is_node_alive == 0)
        {
            printf("node %llx is dead\r\n", p->addr);
            wait_del[del_number] = p;
            del_number ++;
            p = p->nextBrother;
        }
        else 
        {
            p = p->nextBrother;
        }
    }
 }
 
/**
  * @brief  删除死掉的子节点
  * @param  root_itera：整棵树的根节点,用于迭代；
  *         root_r：整棵树的根节点；
  * @retval None
  */
void DeleteDeadNode(TreeNode* root_itera, TreeNode* root_r) {
    uint8_t i;
    TreeNode* result = NULL;
    ScanDeadNode(root_itera, root_r);
    for(i = 0; i < del_number; i ++)
    {
        FindTarget(root_r, wait_del[i]->addr, &result);
        if(result != NULL)
        {
            printf("node %llx is deleted\r\n", result->addr);
            DelSubtree(root_r, result);
        }
    }
    del_number = 0 ;
}

/**
  * @brief  复位所有子节点的存活标志
  * @param  root_itera：整棵树的根节点,用于迭代；
  *         root_r：整棵树的根节点；
  * @retval None
  */
void ReSetNode(TreeNode* root) {
    if (root == NULL) {
        return;
    }

    TreeNode* p = root->firstChild;

    while (p != NULL) {
        ReSetNode(p);
        p->is_node_alive = 0;
        p = p->nextBrother;
    }
}
