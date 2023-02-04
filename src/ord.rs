/* ord.rs
 * Comparison utilities
 * 
 * Copyright 2023 Akash Rawal
 * This file is part of dispatch_ng.
 * 
 * dispatch_ng is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * dispatch_ng is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with dispatch_ng.  If not, see <http://www.gnu.org/licenses/>.
 */

use std::cmp::Ordering;

//Convenience trait for implementing Ord trait.
//If T implements EasyOrd, OrdWrapper<T> implements all required traits.
pub trait EasyOrd {
    fn cmp(&self, other : &Self) -> Ordering;
}

#[derive(Clone)]
pub struct OrdWrapper<T>(pub T);

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
