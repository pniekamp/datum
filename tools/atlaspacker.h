//
// Atlas Packer
//

//
// Copyright (c) 2015 Peter Niekamp
//

#pragma once

#include <vector>
#include <cstdint>

//|---------------------- AtlasPacker ---------------------------------------
//|--------------------------------------------------------------------------

class AtlasPacker
{
  public:

    struct Node
    {
      int id;

      int x, y;
      int width, height;

      size_t children[2] = { 0, 0 };
    };

  public:
    AtlasPacker(int width, int height);

    Node const *find(int id) const;

    Node const *insert(int id, int width, int height);

    int width;
    int height;

  private:

    Node *insert(size_t index, int id, int width, int height);

    std::vector<Node> m_nodes;
};


///////////////////////// AtlasPacker::Constructor //////////////////////////
inline AtlasPacker::AtlasPacker(int width, int height)
  : width(width), height(height)
{
  m_nodes.push_back({ 0, 0, 0, width, height });
}


///////////////////////// AtlasPacker::find /////////////////////////////////
inline AtlasPacker::Node const *AtlasPacker::find(int id) const
{
  for(auto &node : m_nodes)
  {
    if (node.id == id)
      return &node;
  }

  return nullptr;
}


///////////////////////// AtlasPacker::insert ///////////////////////////////
inline AtlasPacker::Node const *AtlasPacker::insert(int id, int width, int height)
{
  return insert(0, id, width, height);
}


///////////////////////// AtlasPacker::insert ///////////////////////////////
inline AtlasPacker::Node *AtlasPacker::insert(size_t index, int id, int width, int height)
{
  Node node = m_nodes[index];

  for(size_t i = 0; i < 2; ++i)
  {
    if (node.children[i] != 0)
    {
      Node *result = insert(node.children[i], id, width, height);

      if (result)
        return result;
    }
  }

  if (node.children[0] != 0 && node.children[1] != 0)
    return nullptr;

  if (node.id != 0)
    return nullptr;

  if (node.width < width || node.height < height)
    return nullptr;

  if (node.width != width || node.height != height)
  {
    if (node.width - width < node.height - height)
    {
      m_nodes.push_back({ 0, node.x + width, node.y, node.width - width, height });
      m_nodes.push_back({ 0, node.x, node.y + height, node.width, node.height - height});
    }
    else
    {
      m_nodes.push_back({ 0, node.x, node.y + height, width, node.height - height });
      m_nodes.push_back({ 0, node.x + width, node.y, node.width - width, node.height });
    }

    m_nodes[index].children[0] = m_nodes.size() - 2;
    m_nodes[index].children[1] = m_nodes.size() - 1;
  }

  m_nodes[index].id = id;
  m_nodes[index].width = width;
  m_nodes[index].height = height;

  return &m_nodes[index];
}
