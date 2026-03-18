
#include "tree_node.h" 
#include "FreeRTOS.h"
#include "task.h"


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
        newNode->expected_next_report_ms = 0;
        if (xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED) {
            newNode->last_seen_tick = (uint32_t)xTaskGetTickCount();
        } else {
            newNode->last_seen_tick = 0;
        }
    }
    return newNode;
}

/* pdMS_TO_TICKS() will overflow on large milliseconds when configTICK_RATE_HZ=1000.
 * Example: 180min = 10800000ms; 10800000*1000 overflows 32-bit, resulting in ~36.8min.
 * Use 64-bit math here to keep long timeout periods correct. */
static TickType_t treenode_ms_to_ticks_safe(uint32_t ms)
{
    uint64_t ticks = ((uint64_t)ms * (uint64_t)configTICK_RATE_HZ) / 1000ULL;
    const uint64_t max_ticks = (uint64_t)((TickType_t)~(TickType_t)0);

    if (ticks > max_ticks)
    {
        ticks = max_ticks;
    }
    if ((ticks == 0) && (ms > 0))
    {
        ticks = 1;
    }
    return (TickType_t)ticks;
}

static uint8_t treenode_is_timed_out(const TreeNode* node, uint32_t now_tick)
{
    uint64_t timeout_ms64 = 0;
    TickType_t timeout_ticks = 0;

    if (node == NULL)
    {
        return 0;
    }

    /* last_seen_tick为0通常意味着尚未被更新；避免误删 */
    if (node->last_seen_tick == 0)
    {
        return 0;
    }

    /* 尚未第一次上报(next_report_ms未知)时：按固定超时保护，避免在第一次上报前被误删 */
    if (node->expected_next_report_ms == 0)
    {
        timeout_ms64 = (uint64_t)ROUTING_CHILD_FIRST_REPORT_TIMEOUT_MS;
        if (timeout_ms64 > 0xFFFFFFFFu)
        {
            timeout_ms64 = 0xFFFFFFFFu;
        }
        timeout_ticks = treenode_ms_to_ticks_safe((uint32_t)timeout_ms64);
        return ((uint32_t)(now_tick - node->last_seen_tick) > (uint32_t)timeout_ticks) ? 1 : 0;
    }

    if (ROUTING_CHILD_TIMEOUT_FACTOR_DEN == 0u)
    {
        return 0;
    }

    timeout_ms64 = ((uint64_t)node->expected_next_report_ms * (uint64_t)ROUTING_CHILD_TIMEOUT_FACTOR_NUM) /
                   (uint64_t)ROUTING_CHILD_TIMEOUT_FACTOR_DEN;
    timeout_ms64 += (uint64_t)ROUTING_CHILD_TIMEOUT_SLACK_MS;

    if (timeout_ms64 > 0xFFFFFFFFu)
    {
        timeout_ms64 = 0xFFFFFFFFu;
    }

    timeout_ticks = treenode_ms_to_ticks_safe((uint32_t)timeout_ms64);
    return ((uint32_t)(now_tick - node->last_seen_tick) > (uint32_t)timeout_ticks) ? 1 : 0;
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

    // 先缓存兄弟节点指针并断开，防止freeTree误把兄弟节点一并释放掉
    TreeNode* next = p->nextBrother;
    p->nextBrother = NULL;

    if (result->firstChild == p) {
        result->firstChild = next;
        freeTree(p);
        return;
    }

    TreeNode* q = result->firstChild;

    while (q != NULL && q->nextBrother != p) {
        q = q->nextBrother;
    }

    if (q != NULL) {
        q->nextBrother = next;
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
    uint16_t guard = 0;
    if (father == NULL || child == NULL || father == child) {
        return;
    }
    if(father->firstChild == NULL)//如果父节点没有子节点
    {
        child->nextBrother = NULL;
        father->firstChild = child;  
        return;
    }
    p = father->firstChild;
    while (p != NULL)//退出循环时p指向最右边的子节点
    {
        if (p == child) { // 已经挂载过，避免形成环
            return;
        }
        if (p->addr == child->addr) { // 同地址重复添加，释放新节点避免链表越来越长
            child->nextBrother = NULL;
            freeTree(child);
            return;
        }
        if (p->nextBrother == NULL) {
            break;
        }
        p = p->nextBrother;
        if (++guard > 512) { // 兄弟链表异常(可能成环)，避免死循环
            child->nextBrother = NULL;
            freeTree(child);
            return;
        }
    }
    if (p == NULL) {
        child->nextBrother = NULL;
        father->firstChild = child;
        return;
    }
    child->nextBrother = NULL;
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

static uint8_t TreeContainsPointer(TreeNode* root, TreeNode* target)
{
    TreeNode* p;
    uint16_t guard = 0;

    if ((root == NULL) || (target == NULL))
    {
        return 0;
    }
    if (root == target)
    {
        return 1;
    }

    p = root->firstChild;
    while (p != NULL)
    {
        if (TreeContainsPointer(p, target))
        {
            return 1;
        }
        if (p->nextBrother == p)
        {
            break;
        }
        p = p->nextBrother;
        if (++guard > 512)
        {
            break;
        }
    }
    return 0;
}

/**
  * @brief  修改子节点,将子节点及其子树添加到新的父节点下，并断开与旧的父节点的联系
  * @param  rootnode：整棵树的根节点；newfather：新的父节点；child：指向待操作的节点的指针
  * @retval None
  */
void ChangeChild(TreeNode* rootnode, TreeNode* newfather, TreeNode* child)
{
    TreeNode* p;
    uint16_t guard = 0;

    if ((rootnode == NULL) || (newfather == NULL) || (child == NULL) || (newfather == child))
    {
        return;
    }

    /* 防止把节点挂到自己的子树下面形成环。 */
    if (TreeContainsPointer(child, newfather))
    {
        return;
    }

    Disconnect(rootnode, child);

    if(newfather->firstChild == NULL)//如果父节点没有子节点
    {
        child->nextBrother = NULL;
        newfather->firstChild = child;
        return;
    }
    p = newfather->firstChild;
    while (p->nextBrother)//退出循环时p指向最右边的子节点
    {
        if (p == child)
        {
            child->nextBrother = NULL;
            return;
        }
        if (p->nextBrother == p)
        {
            return;
        }
        p = p->nextBrother;
        if (++guard > 512)
        {
            return;
        }
    }
    if (p == child)
    {
        child->nextBrother = NULL;
        return;
    }
    child->nextBrother = NULL;
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
    uint16_t guard = 0;

    while (p != NULL) {
        TraverseTree(p);
        printf("Child Node ADDR: %llx, is alive? %x\r\n", p->addr, p->is_node_alive);
        if (p->nextBrother == p) {
            printf("TraverseTree error: nextBrother loop at %llx\r\n", p->addr);
            break;
        }
        p = p->nextBrother;
        if (++guard > 1024) {
            printf("TraverseTree aborted: too many brothers(loop?)\r\n");
            break;
        }
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

static void ScanDeadNodeInternal(TreeNode* root_itera, TreeNode* root_r, uint32_t now_tick) {
      if (root_itera == NULL) {
          return;
      }
  
     TreeNode* p = root_itera->firstChild;
     while (p != NULL) {
         ScanDeadNodeInternal(p, root_r, now_tick);
          if(treenode_is_timed_out(p, now_tick))
          {
              printf("node %llx is dead\r\n", p->addr);
              if (del_number < (uint8_t)(sizeof(wait_del) / sizeof(wait_del[0]))) {
                  wait_del[del_number] = p;
                  del_number ++;
             }
             p = p->nextBrother;
         }
         else 
         {
            p = p->nextBrother;
         }
     }
  }

void ScanDeadNode(TreeNode* root_itera, TreeNode* root_r) {
    uint32_t now_tick = 0;
    if (xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED) {
        now_tick = (uint32_t)xTaskGetTickCount();
    }
    ScanDeadNodeInternal(root_itera, root_r, now_tick);
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
