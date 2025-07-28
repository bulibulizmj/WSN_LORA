#ifndef TREE_NODE_H
#define TREE_NODE_H

#include "sys.h"
#include "mac.h"

// 定义树节点
typedef struct TreeNode {
    NodeAddr addr;                  //节点的mac地址
    uint8_t is_node_alive;          //子节点存活标志，为1节点存活，为0节点死亡，如果一段时间内没有收到节点的信息则认为节点死亡
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



