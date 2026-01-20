#ifndef TREE_NODE_H
#define TREE_NODE_H

#include <stdint.h>
#include "sys.h"
#include "mac.h"
#include "adaptive_report.h"

/* 子节点动态超时判活参数（仅在收到上报包携带next_report_ms时生效）判死阈值(ms) = expected_next_report_ms * NUM / DEN + SLACK_MS */
#ifndef ROUTING_CHILD_TIMEOUT_FACTOR_NUM
#define ROUTING_CHILD_TIMEOUT_FACTOR_NUM 3u
#endif

#ifndef ROUTING_CHILD_TIMEOUT_FACTOR_DEN
#define ROUTING_CHILD_TIMEOUT_FACTOR_DEN 2u
#endif

#ifndef ROUTING_CHILD_TIMEOUT_SLACK_MS
#define ROUTING_CHILD_TIMEOUT_SLACK_MS (5u * 60u * 1000u)
#endif

/* 子节点尚未第一次上报(next_report_ms未知)时的超时保护：
 * 避免node_check周期较短时，节点在第一次上报前被误删。
 * 默认给到Tmax + slack。 */
#ifndef ROUTING_CHILD_FIRST_REPORT_TIMEOUT_MS
#define ROUTING_CHILD_FIRST_REPORT_TIMEOUT_MS \
    ((uint32_t)ADAPT_REPORT_TMAX_MINUTES * 60u * 1000u + (uint32_t)ROUTING_CHILD_TIMEOUT_SLACK_MS)
#endif

// 定义树节点
typedef struct TreeNode {
    NodeAddr addr;                  //节点的mac地址
    uint8_t is_node_alive;          //子节点存活标志，为1节点存活，为0节点死亡，如果一段时间内没有收到节点的信息则认为节点死亡
    uint32_t last_seen_tick;        //最后一次“看到该节点”的系统tick
    uint32_t expected_next_report_ms; //该节点声明的下一次上报间隔(ms)，0表示未知/未上报
    struct TreeNode* firstChild;
    struct TreeNode* nextBrother;
} TreeNode;


TreeNode* createNode(NodeAddr addr);                                        // 创建树节点
void freeTree(TreeNode* root);                                              // 释放树节点及其子树
void FindFather(TreeNode* t, TreeNode* p, TreeNode** result);               // 算法 FindFather在以t为根的树中寻找p的子节点
void DelSubtree(TreeNode* t, TreeNode* p);                                  // 删除一个节点及以其为跟的子树。t：指向树的根节点的指针；p：指向子树根节点的指针
void FindTarget(TreeNode* t, NodeAddr addr, TreeNode** result);             // 给定节点mac地址返回对应节点指针并存入result
void AddChild(TreeNode* father, TreeNode* child);                           // 为某个节点添加子节点
void ChangeChild(TreeNode* rootnode, TreeNode* newfather, TreeNode* child); // 修改子节点,将子节点及其子树添加到新的父节点下，并断开与旧的父节点的联系
void Disconnect(TreeNode* rootnode, TreeNode* child);                       // 断开与父节点的联系,但不释放子节点内存
void TraverseTree(TreeNode* root);                                          // 遍历所有节点，打印整棵树的子节点，不包括根节点
void ScanDeadNode(TreeNode* root_itera, TreeNode* root_r);
void DeleteDeadNode(TreeNode* root_itera, TreeNode* root_r);
void ReSetNode(TreeNode* root);
#endif


