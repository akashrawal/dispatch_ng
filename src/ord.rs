//Commonly used utilities

use std::cmp::Ordering;

//Convenience trait for implementing Ord trait.
//If T implements EasyOrd, OrdWrapper<T> implements all required traits.
pub trait EasyOrd {
    fn cmp(&self, other : &Self) -> Ordering;
}

#[derive(Clone)]
pub struct OrdWrapper<T>(pub T);

/*
impl<T : Clone> Clone for OrdWrapper<T> {
    fn clone(&self) -> Self {
        Self(self.0.clone())        
    }
}
*/

impl<T : EasyOrd> PartialEq for OrdWrapper<T> {
    fn eq(&self, other: &Self) -> bool {
        self.0.cmp(&other.0) == Ordering::Equal
    }
}

impl<T : EasyOrd> Eq for OrdWrapper<T> {
}

impl<T : EasyOrd> PartialOrd for OrdWrapper<T> {
    fn partial_cmp(&self, other: &Self) -> Option<Ordering> {
        Some(self.0.cmp(&other.0))
    }
}

impl<T : EasyOrd> Ord for OrdWrapper<T> {
    fn cmp(&self, other: &Self) -> Ordering {
        self.0.cmp(&other.0)
    }
}
