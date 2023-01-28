//A kind of heap that can be updated, for load balancing

use std::cmp::Reverse;
use std::hash::Hash;
use std::collections::{ BinaryHeap, HashMap };

pub trait HashAndOrd : Hash + PartialEq + Eq + PartialOrd + Ord + Clone{
}
impl<T : Hash + PartialEq + Eq + PartialOrd + Ord + Clone> HashAndOrd for T {
}

//Entry type
#[derive(Hash, PartialEq, Eq, PartialOrd, Ord)]
struct Entry<E : HashAndOrd> {
    pub use_count : usize,
    pub value : E,
}

//Heap type
pub struct BalanceHeap<E : HashAndOrd> {
    heap : BinaryHeap<Reverse<Entry<E>>>,
    map : HashMap<E, usize>,
}

impl<E : HashAndOrd> BalanceHeap<E> {
    pub fn new() -> Self {
        Self {
            heap : BinaryHeap::new(),
            map : HashMap::new(),
        }
    }

    pub fn pop(&mut self) -> Option<(E, usize)> {
        while let Some(Reverse(entry)) = self.heap.pop() {
            //Check whether the entry exists with correct use count
            if self.map.get(&entry.value) == Some(&entry.use_count) {
                self.map.remove(&entry.value);
                return Some((entry.value, entry.use_count));
            }
        }
        None
    }

    pub fn get(&mut self, value : &E) -> Option<usize> {
        self.map.get(value).copied()
    }
    
    pub fn update(&mut self, value : E, use_count : usize) {
        self.map.insert(value.clone(), use_count); 
        if self.heap.len() > 16 && self.heap.len() > (2 * self.map.len()) {
            //Garbage collection
            let heap_contents : Vec<Reverse<Entry<E>>> = self.map.iter()
                .map(|(v, c)| Reverse(Entry { 
                    value : v.clone(), 
                    use_count : *c
                }))
                .collect();
            self.heap = heap_contents.into();
        } else {
            self.heap.push(Reverse(Entry { value, use_count }));
        }
    }
}

#[cfg(test)]
mod test {
    use super::*;

    #[test]
    fn test_basic() {
        let mut heap = BalanceHeap::<char>::new();
        heap.update('A', 1);
        heap.update('B', 2);

        assert_eq!(heap.pop(), Some(('A', 1)));
        assert_eq!(heap.pop(), Some(('B', 2)));
        assert_eq!(heap.pop(), None);
    }

    #[test]
    fn test_update() {
        let mut heap = BalanceHeap::<char>::new();
        heap.update('A', 1);
        heap.update('B', 2);
        heap.update('A', 3);

        assert_eq!(heap.pop(), Some(('B', 2)));
        assert_eq!(heap.pop(), Some(('A', 3)));
        assert_eq!(heap.pop(), None);
    }

    #[test]
    fn test_gc() {
        let mut heap = BalanceHeap::<char>::new();
        heap.update('A', 1);
        heap.update('B', 2);

        for count in 3..=24 {
            heap.update('A', count);
        }

        assert_eq!(heap.heap.len(), 8);
        assert_eq!(heap.pop(), Some(('B', 2)));
        assert_eq!(heap.pop(), Some(('A', 24)));
        assert_eq!(heap.pop(), None);
    }
}
