//Load balancer algorithm, selects the outgoing IP address with minimum 
//use count.

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

fn update_use_count<T : Hash + Eq + Clone>(
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

impl Drop for IpGuard {
    fn drop(&mut self) {
        let mut lock = self.balancer.0.lock().unwrap();
        match self.addr {
            IpAddr::V4(addr) => update_use_count(&mut lock.v4_heap, addr, -1),
            IpAddr::V6(addr) => update_use_count(&mut lock.v6_heap, addr, -1),
        }
    }
}

impl Balancer {
    fn take(&self, take_v4 : bool, take_v6 : bool) -> Option<IpGuard> {
        let addr = {
            let mut lock = self.0.lock().unwrap();
            let mut list = Vec::<(IpAddr, OrdWrapper<MetricState>)>::new();
            if take_v4 {
                if let Some((addr, met)) = lock.v4_heap.peek() {
                    list.push((IpAddr::V4(addr), met.clone()));
                }
            }
            if take_v6 {
                if let Some((addr, met)) = lock.v6_heap.peek() {
                    list.push((IpAddr::V6(addr), met.clone()));
                }
            }
            list.sort_by(|a, b| a.1.cmp(&b.1));

            let addr = list.last()?.0.clone();
            match addr {
                IpAddr::V4(v4addr) => {
                    update_use_count(&mut lock.v4_heap, v4addr, 1);
                },
                IpAddr::V6(v6addr) => {
                    update_use_count(&mut lock.v6_heap, v6addr, 1);
                },
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

#[cfg(test)]
mod test {
    use super::*;

    #[test]
    fn test() {
        let addr1 = IpAddr::from([172, 16, 0, 1]);
        let addr2 = IpAddr::from([172, 16, 0, 2]);

        let balancer = Balancer::new([ (addr1, 1), (addr2, 2) ].into_iter().collect());

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
