//! Bit-exact safe port of OpenMalaria's `Xoshiro256P` generator (`model/util/xoshiro.h`).

use crate::RngCore;
use serde::{Deserialize, Serialize};

const DEFAULT_S1: u64 = 0x220b_5e8d;
const DEFAULT_S2: u64 = 0x712a_58a2;
const DEFAULT_S3: u64 = 0x712a_58a2;
const DOUBLE_SCALE: f64 = 1.110_223_024_625_156_5e-16; // 2^-53

/// 256-bit `xoshiro256+` generator used for per-human and per-subsystem local RNGs.
///
/// # Examples
/// ```
/// use om_rng::{OmXoshiro256Plus, RngCore};
/// let mut rng = OmXoshiro256Plus::from_words(1, 2, 3, 4);
/// assert_eq!(rng.next_u64(), 5);
/// assert_eq!(rng.next_u64(), 211_106_232_532_999);
/// ```
#[derive(Clone, Copy, Debug, PartialEq, Eq, Serialize, Deserialize)]
pub struct OmXoshiro256Plus {
    s: [u64; 4],
}

impl OmXoshiro256Plus {
    /// Constructs a generator from explicit 4x64-bit state words.
    #[must_use]
    pub const fn from_words(a: u64, b: u64, c: u64, d: u64) -> Self {
        Self { s: [a, b, c, d] }
    }

    /// Constructs a generator matching C++ `RNG<Xoshiro256P>(uint64_t seed, uint64_t stream)`.
    #[must_use]
    pub const fn from_seed_stream(seed: u64, stream: u64) -> Self {
        Self {
            s: [seed, stream, DEFAULT_S2, DEFAULT_S3],
        }
    }

    /// Constructs a generator from a single 64-bit word using OpenMalaria's default `b, c, d` words.
    #[must_use]
    pub const fn from_single_word(a: u64) -> Self {
        Self {
            s: [a, DEFAULT_S1, DEFAULT_S2, DEFAULT_S3],
        }
    }

    /// Seeds a local `Xoshiro256+` by drawing four `gen_u64()` words from a parent `RngCore` (e.g. `MasterRng`).
    #[must_use]
    pub fn from_parent<P: RngCore>(parent: &mut P) -> Self {
        let s0 = parent.gen_u64();
        let s1 = parent.gen_u64();
        let s2 = parent.gen_u64();
        let s3 = parent.gen_u64();
        Self {
            s: [s0, s1, s2, s3],
        }
    }

    /// Re-seeds with `(seed, stream)` using OpenMalaria's default `c` and `d` words.
    pub fn seed_stream(&mut self, seed: u64, stream: u64) {
        self.s = [seed, stream, DEFAULT_S2, DEFAULT_S3];
    }

    /// Re-seeds from a parent `RngCore` via four `gen_u64()` draws.
    pub fn seed_from_parent<P: RngCore>(&mut self, parent: &mut P) {
        self.s[0] = parent.gen_u64();
        self.s[1] = parent.gen_u64();
        self.s[2] = parent.gen_u64();
        self.s[3] = parent.gen_u64();
    }

    /// Returns the raw `[u64; 4]` state words for binary checkpointing.
    #[must_use]
    pub const fn state(&self) -> [u64; 4] {
        self.s
    }

    /// Restores the raw `[u64; 4]` state words from a binary checkpoint.
    pub fn set_state(&mut self, state: [u64; 4]) {
        self.s = state;
    }

    /// Steps the `xoshiro256+` recurrence and returns the 64-bit output `s[0] + s[3]`.
    #[inline]
    pub fn next_u64(&mut self) -> u64 {
        let result = self.s[0].wrapping_add(self.s[3]);
        let t = self.s[1] << 17;

        self.s[2] ^= self.s[0];
        self.s[3] ^= self.s[1];
        self.s[1] ^= self.s[2];
        self.s[0] ^= self.s[3];

        self.s[2] ^= t;
        self.s[3] = self.s[3].rotate_left(45);

        result
    }
}

impl RngCore for OmXoshiro256Plus {
    #[inline]
    fn gen_u32(&mut self) -> u32 {
        (self.next_u64() >> 32) as u32
    }

    #[inline]
    fn gen_u64(&mut self) -> u64 {
        self.next_u64()
    }

    #[inline]
    fn gen_double(&mut self) -> f64 {
        ((self.next_u64() >> 11) as f64) * DOUBLE_SCALE
    }
}
