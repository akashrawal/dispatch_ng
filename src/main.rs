mod ord;
mod balance_heap;
mod balancer;
mod session;

use std::borrow::Cow;

use std::str::FromStr;

use std::net::IpAddr;

use clap::Parser;

use futures::prelude::*;
use futures::stream::FuturesUnordered;

use tokio::task::JoinHandle;
use tokio::net::TcpListener;

use balancer::Balancer;

#[derive(Debug, Parser)]
struct Args {
    /// Number of threads to use in the Async runtime. Default value of 1
    /// works well in all cases, unless you have multiple 10G or above 
    /// connections for some reason... Just leave it at 1. 
    #[arg(long, default_value_t = 1)]
    threads : usize, 

    /// Addresses to offer SOCKS5 proxy. If not specified, it listens on 
    /// localhost. 
    #[arg(short, long)]
    listen : Vec<Cow<'static, str>>,

    /// List of addresses to load-balance across.
    addrs : Vec<String>, 
}

//Async entry point
async fn async_main(args : Args) {
    //Default listening addresses
    let listen_addrs = if args.listen.is_empty() {
        vec!["127.0.0.1:1080".into(), "[::1]:1080".into()]    
    } else {
        args.listen
    };

    enum ListenErr {
        Bind(std::io::Error),
        Accept(std::io::Error),
    }

    //Create the load balancer
    let mut addrs = Vec::<IpAddr>::new();
    for addr in args.addrs.into_iter() {
        addrs.push(IpAddr::from_str(&addr)
                   .unwrap_or_else(|e| panic!("Unable to parse address {}: {}", 
                                              addr, e)));
    }
    let balancer = Balancer::new(addrs.into_iter().collect());

    //Start listening tasks
    let mut listeners : FuturesUnordered<JoinHandle<()>> 
        = listen_addrs.into_iter()
        .map( |addr| tokio::spawn(listen(addr, balancer.clone())) )
        .collect();

    while let Some(result) = listeners.next().await {
        match result {
            Ok(_) => (),
            Err(e) => panic!("Unable to wait for listeners: {}", e),
        }
    }

    panic!("No addresses available for listening");
}

//Listener task
async fn listen(addr : Cow<'static, str>, balancer : Balancer) {
    let listener = TcpListener::bind(addr.as_ref()).await
        .unwrap_or_else(|e| {
            panic!("Unable to bind SOCKS5 listener at address {}: {}",
                   addr, e)
        });

    loop {
        let (stream, remote_addr) = listener.accept()
            .await
            .expect("Unable to accept new connection");
    
        tokio::spawn(session::enter(stream, remote_addr, balancer.clone()));
    }
}


//Minimal main function
fn main() {
    env_logger::init();

    //Read the argument vector
    let args = Args::parse();
    log::debug!("Args: {:?}", args);

    //Create and launch runtime
    let runtime = if args.threads == 1 {
        tokio::runtime::Builder::new_current_thread()
            .enable_io()
            .build()
    } else {
        tokio::runtime::Builder::new_multi_thread()
            .worker_threads(args.threads)
            .enable_io()
            .build()
    };
    let runtime = runtime.expect("Unable to create new runtime");
    runtime.block_on(async_main(args));
}
