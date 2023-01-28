//Load balancer algorithm, selects the outgoing IP address with minimum 
//use count.

use std::sync::Arc;
use std::sync::Mutex;

use std::net::{ IpAddr, Ipv4Addr, Ipv6Addr };

use crate::balance_heap::BalanceHeap;


//Balancer state
pub struct BalancerState {
    v4_heap : BalanceHeap<Ipv4Addr>, 
    v6_heap : BalanceHeap<Ipv6Addr>, 
}

impl BalancerState {
    pub fn new() -> Self {
        Self {
            v4_heap: BalanceHeap::new(), 
            v6_heap: BalanceHeap::new() 
        }
    }
}

impl Extend<IpAddr> for BalancerState {
    fn extend<T: IntoIterator<Item = IpAddr>>(&mut self, iter: T) {
        for addr in iter {
            match addr {
                IpAddr::V4(v4addr) => self.v4_heap.update(v4addr, 0),
                IpAddr::V6(v6addr) => self.v6_heap.update(v6addr, 0),
            }
        }
    }
}

//Public API
#[derive(Clone)]
pub struct Balancer(Arc<Mutex<BalancerState>>);

impl From<BalancerState> for Balancer {
    fn from(value: BalancerState) -> Self {
        Self(Arc::new(Mutex::new(value)))
    }
}

pub struct IpGuard {
    balancer : Balancer,
    addr : IpAddr,
}

impl Drop for IpGuard {
    fn drop(&mut self) {
        let mut lock = self.balancer.0.lock().unwrap();
        match self.addr {
            IpAddr::V4(v4addr) => {
                let count = lock.v4_heap.get(&v4addr).unwrap();
                lock.v4_heap.update(v4addr, count - 1);
            },
            IpAddr::V6(v6addr) => {
                let count = lock.v6_heap.get(&v6addr).unwrap();
                lock.v6_heap.update(v6addr, count - 1);
            },
        }
    }
}

impl Balancer {
    fn take(&self, take_v4 : bool, take_v6 : bool) -> Option<IpGuard> {
        let addr = {
            let mut lock = self.0.lock().unwrap();
            let mut list = Vec::<(IpAddr, usize)>::new();
            if take_v4 {
                if let Some((addr, count)) = lock.v4_heap.pop() {
                    list.push((IpAddr::V4(addr), count));
                }
            }
            if take_v6 {
                if let Some((addr, count)) = lock.v6_heap.pop() {
                    list.push((IpAddr::V6(addr), count));
                }
            }
            list.sort_by(|a, b| a.1.cmp(&b.1));

            let mut update_heap = |addr : IpAddr, count : usize| {
                match addr {
                    IpAddr::V4(v4addr) => lock.v4_heap.update(v4addr, count),
                    IpAddr::V6(v6addr) => lock.v6_heap.update(v6addr, count),
                }
            };

            let mut iter = list.into_iter();
            let (addr, count) = iter.next()?;
            update_heap(addr, count + 1);
            for (sec_addr, sec_count) in iter {
                update_heap(sec_addr, sec_count);
            }

            addr
        };

        Some(IpGuard {
            balancer : self.clone(),
            addr
        })
    }

    fn take_v4(&self) -> Option<(IpGuard, Ipv4Addr)> {
        let guard = self.take(true, false)?;
        let addr = match guard.addr {
            IpAddr::V4(addr) => addr,
            _ => return None,
        };
        Some((guard, addr))
    }

    fn take_v6(&self) -> Option<(IpGuard, Ipv6Addr)> {
        let guard = self.take(true, false)?;
        let addr = match guard.addr {
            IpAddr::V6(addr) => addr,
            _ => return None,
        };
        Some((guard, addr))
    }
}
