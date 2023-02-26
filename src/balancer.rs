/* balancer.rs
 * Load balancer algorithm, selects the outgoing IP address with minimum 
 * use count.
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

use std::hash::Hash;

use std::sync::Arc;
use std::sync::Mutex;

use std::net::{ IpAddr, Ipv4Addr, Ipv6Addr };

use crate::ord::{ OrdWrapper, EasyOrd };
use crate::balance_heap::BalanceHeap;


//Metric state (stores use count and provides comparison function)
#[derive(Clone)]
pub struct MetricState {
    use_count : usize,
    metric : usize,
}

impl EasyOrd for MetricState {
    fn cmp(&self, other: &Self) -> std::cmp::Ordering {
        let self_ord = self.use_count / self.metric;
        let other_ord = other.use_count / other.metric;
        other_ord.cmp(&self_ord) //< Reverse order
    }
}


//Balancer state
pub struct BalancerState {
    v4_heap : BalanceHeap<Ipv4Addr, OrdWrapper<MetricState>>, 
    v6_heap : BalanceHeap<Ipv6Addr, OrdWrapper<MetricState>>, 
}

impl<T> FromIterator<T> for BalancerState 
where Self : Extend<T> {
    fn from_iter<U: IntoIterator<Item = T>>(iter: U) -> Self {
        let mut res = Self {
            v4_heap : BalanceHeap::new(),
            v6_heap : BalanceHeap::new(),
        };
        res.extend(iter);
        res
    }
}

impl Extend<(IpAddr, usize)> for BalancerState {
    fn extend<T: IntoIterator<Item = (IpAddr, usize)>>(&mut self, iter: T) {
        for (addr, metric) in iter {
            let order_value = OrdWrapper(MetricState {
                use_count : 0,
                metric,
            });
            match addr {
                IpAddr::V4(v4addr) => self.v4_heap.update(v4addr, order_value),
                IpAddr::V6(v6addr) => self.v6_heap.update(v6addr, order_value),
            } 
        }
    }
}

impl Extend<IpAddr> for BalancerState {
    fn extend<T: IntoIterator<Item = IpAddr>>(&mut self, iter: T) {
        self.extend(iter.into_iter().map(|addr| (addr, 1)))
    }
}

impl BalancerState {
    fn chg_use_count(&mut self, addr : IpAddr, delta : isize) {
        fn update_heap<T : Hash + Eq + Clone>(
            heap : &mut BalanceHeap<T, OrdWrapper<MetricState>>, 
            value : T,
            delta : isize
        ) {
            let OrdWrapper(MetricState { use_count, metric }) 
                = heap.get(&value).unwrap().clone();
            heap.update(value, OrdWrapper(MetricState { 
                use_count : ((use_count as isize) + delta) as usize, 
                metric,
            }));
        }

        match addr {
            IpAddr::V4(addr) => update_heap(&mut self.v4_heap, addr, delta),
            IpAddr::V6(addr) => update_heap(&mut self.v6_heap, addr, delta),
        }
    }
}

//Balancer
#[derive(Clone)]
pub struct Balancer(Arc<Mutex<BalancerState>>);

impl Balancer {
    pub fn new(state : BalancerState) -> Self {
        Self(Arc::new(Mutex::new(state)))
    }
}

pub struct IpGuard {
    balancer : Balancer,
    addr : IpAddr,
}

impl IpGuard {
    pub fn get_addr(&self) -> IpAddr {
        self.addr
    }
}


impl Drop for IpGuard {
    fn drop(&mut self) {
        let mut lock = self.balancer.0.lock().unwrap();
        lock.chg_use_count(self.addr, -1);
    }
}

impl Balancer {
    pub fn take(&self, take_v4 : bool, take_v6 : bool) -> Option<IpGuard> {
        let addr = {
            let mut lock = self.0.lock().unwrap();
            let mut list = Vec::<(IpAddr, OrdWrapper<MetricState>)>::new();
            if take_v4 {
                if let Some((addr, met)) = lock.v4_heap.peek() {
                    list.push((IpAddr::V4(addr), met));
                }
            }
            if take_v6 {
                if let Some((addr, met)) = lock.v6_heap.peek() {
                    list.push((IpAddr::V6(addr), met));
                }
            }
            list.sort_by(|a, b| a.1.cmp(&b.1));

            let addr = list.last()?.0; 
            lock.chg_use_count(addr, 1);

            addr
        };

        Some(IpGuard {
            balancer : self.clone(),
            addr
        })
    }
}

#[cfg(test)]
mod test {
    use super::*;

    #[test]
    fn test() {
        let addr1 = IpAddr::from([172, 16, 0, 1]);
        let addr2 = IpAddr::from([172, 16, 0, 2]);

        let balancer = Balancer::new([ (addr1, 1), (addr2, 2) ]
                                     .into_iter().collect());

        let res1 = balancer.take(true, true);
        let res2 = balancer.take(true, true);
        let res3 = balancer.take(true, true);

        let vec : Vec<IpAddr> = res1.into_iter().chain(res2).chain(res3)
            .map(|guard| guard.addr)
            .collect();

        assert_eq!(vec.iter().filter(|x| **x == addr1).count(), 1);
        assert_eq!(vec.iter().filter(|x| **x == addr2).count(), 2);
    }
}
