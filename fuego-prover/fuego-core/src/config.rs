//! LP Pool Configuration
//!
//! Testnet and production deployment configurations.

use serde::{Deserialize, Serialize};

#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize)]
pub enum Network {
    Testnet,
    Mainnet,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct PoolConfig {
    pub network: Network,
    pub epoch_blocks: u32,
    pub min_liquidity: u64,
    pub fee_bps: u16,
    pub max_events_per_epoch: u32,
    pub proving_timeout_secs: u64,
}

impl PoolConfig {
    pub fn testnet() -> Self {
        Self {
            network: Network::Testnet,
            epoch_blocks: 100,
            min_liquidity: 1000,
            fee_bps: 30,
            max_events_per_epoch: 10000,
            proving_timeout_secs: 600,
        }
    }

    pub fn mainnet() -> Self {
        Self {
            network: Network::Mainnet,
            epoch_blocks: 100,
            min_liquidity: 10000,
            fee_bps: 30,
            max_events_per_epoch: 100000,
            proving_timeout_secs: 300,
        }
    }
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ProverConfig {
    pub pool_id: String,
    pub rpc_url: String,
    pub proof_output_dir: String,
    pub verification_key_path: String,
    pub celery_broker_url: Option<String>,
}

impl ProverConfig {
    pub fn testnet(pool_id: &str) -> Self {
        Self {
            pool_id: pool_id.to_string(),
            rpc_url: "http://localhost:8337".to_string(),
            proof_output_dir: "/tmp/lp_proofs".to_string(),
            verification_key_path: "vk.bin".to_string(),
            celery_broker_url: None,
        }
    }

    pub fn mainnet(pool_id: &str) -> Self {
        Self {
            pool_id: pool_id.to_string(),
            rpc_url: "https://api.fuego.network".to_string(),
            proof_output_dir: "/var/lib/fuego/proofs".to_string(),
            verification_key_path: "/etc/fuego/vk.bin".to_string(),
            celery_broker_url: Some("redis://localhost:6379".to_string()),
        }
    }
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct MonitoringConfig {
    pub prometheus_port: u16,
    pub metrics_interval_secs: u64,
    pub alert_webhook_url: Option<String>,
}

impl MonitoringConfig {
    pub fn testnet() -> Self {
        Self {
            prometheus_port: 9090,
            metrics_interval_secs: 60,
            alert_webhook_url: None,
        }
    }

    pub fn mainnet() -> Self {
        Self {
            prometheus_port: 9090,
            metrics_interval_secs: 30,
            alert_webhook_url: Some("https://alerts.fuego.network/webhook".to_string()),
        }
    }
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct DeploymentConfig {
    pub pool: PoolConfig,
    pub prover: ProverConfig,
    pub monitoring: MonitoringConfig,
}

impl DeploymentConfig {
    pub fn testnet(pool_id: &str) -> Self {
        Self {
            pool: PoolConfig::testnet(),
            prover: ProverConfig::testnet(pool_id),
            monitoring: MonitoringConfig::testnet(),
        }
    }

    pub fn mainnet(pool_id: &str) -> Self {
        Self {
            pool: PoolConfig::mainnet(),
            prover: ProverConfig::mainnet(pool_id),
            monitoring: MonitoringConfig::mainnet(),
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_testnet_config() {
        let cfg = DeploymentConfig::testnet("test_pool");
        assert_eq!(cfg.pool.network, Network::Testnet);
        assert_eq!(cfg.prover.rpc_url, "http://localhost:8337");
    }

    #[test]
    fn test_mainnet_config() {
        let cfg = DeploymentConfig::mainnet("main_pool");
        assert_eq!(cfg.pool.network, Network::Mainnet);
        assert!(cfg.monitoring.alert_webhook_url.is_some());
    }
}
