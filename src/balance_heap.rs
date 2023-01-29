//A kind of heap that can be updated, for load balancing

use std::hash::Hash;
use std::collections::{ BinaryHeap, HashMap };

use crate::ord::{ EasyOrd, OrdWrapper };

//Entry type
struct HeapEnt<E, V> {
    entry : E,
    order : V,
}

impl<E, V : Ord> EasyOrd for HeapEnt<E, V> {
    fn cmp(&self, other : &Self) -> std::cmp::Ordering {
        self.order.cmp(&other.order)
    }
}


//Heap type
pub struct BalanceHeap<E : Hash, V : Ord> {
    heap : BinaryHeap<OrdWrapper<HeapEnt<E, V>>>,
    map : HashMap<E, V>,
}

impl<E : Hash + Eq + Clone, V : Ord + Clone> BalanceHeap<E, V> {
    pub fn new() -> Self {
        Self {
            heap : BinaryHeap::new(),
            map : HashMap::new(),
        }
    }

    pub fn peek(&mut self) -> Option<(E, V)> {
        while let Some(OrdWrapper(el)) = self.heap.peek() {
            //Check whether the entry exists with correct order value
            if self.map.get(&el.entry) == Some(&el.order) {
                return Some((el.entry.clone(), el.order.clone()));
            }
            self.heap.pop();
        }
        None
    }

    pub fn get(&mut self, entry : &E) -> Option<&V> {
        self.map.get(entry)
    }
    
    pub fn update(&mut self, entry : E, order : V) {
        self.map.insert(entry.clone(), order.clone()); 
        if self.heap.len() > 16 && self.heap.len() > (2 * self.map.len()) {
            //Garbage collection
            let heap_contents : Vec<OrdWrapper<HeapEnt<E, V>>> = self.map.iter()
                .map(|(e, v)| OrdWrapper(HeapEnt { 
                    entry : e.clone(), 
                    order : v.clone()
                }))
                .collect();
            self.heap = heap_contents.into();
        } else {
            self.heap.push(OrdWrapper(HeapEnt { entry, order }));
        }
    }
}

#[cfg(test)]
mod test {
    use super::*;

    #[test]
    fn test_basic() {
        let mut heap = BalanceHeap::<char, u32>::new();
        heap.update('A', 1);
        heap.update('B', 2);

        assert_eq!(heap.peek(), Some(('B', 2)));
    }

    #[test]
    fn test_update() {
        let mut heap = BalanceHeap::<char, u32>::new();
        heap.update('A', 1);
        heap.update('B', 2);
        heap.update('A', 3);

        assert_eq!(heap.peek(), Some(('A', 3)));
    }

    #[test]
    fn test_gc() {
        let mut heap = BalanceHeap::<char, u32>::new();
        heap.update('A', 1);
        heap.update('B', 2);

        for count in 3..=24 {
            heap.update('A', count);
        }

        assert_eq!(heap.heap.len(), 8);
        assert_eq!(heap.peek(), Some(('A', 24)));
    }
}
