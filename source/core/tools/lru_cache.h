#ifndef CORE_TOOLS_LRU_CACHE_H
#define CORE_TOOLS_LRU_CACHE_H

#include <cstdint>
#include <list>
#include <unordered_map>

namespace fantasy 
{
    template <typename T>
    class LruCache 
    {
    public:
        explicit LruCache(uint32_t capacity) : _capacity(capacity) {}
        
        T* get(uint32_t key) 
        {
            auto iter = _map.find(key);
            if(iter == _map.end())
            {
                return nullptr;
            }
            else
            {
                _list.splice(_list.begin(), _list, iter->second);
                auto data = _map[key];
                return &data->second;
            }
        }
        
        void insert(uint32_t key, const T& value) 
        {
            auto iter = _map.find(key);
            if(iter == _map.end())
            {
                _list.push_front(std::make_pair(key, value));
                _map.insert(std::make_pair(key, _list.begin()));
                if(_list.size() > _capacity)
                {
                    auto data = &_list.back();
                    _map.erase(data->first);
                    _list.pop_back();
                }
            }
            else
            {
                iter->second->second = value;
                _list.splice(_list.begin(), _list, iter->second);
            }
        }

    private:
        uint32_t _capacity;
        std::unordered_map<uint32_t,  typename std::list<std::pair<uint32_t, T>>::iterator> _map;
        std::list<std::pair<uint32_t, T>> _list;
    };
}








#endif