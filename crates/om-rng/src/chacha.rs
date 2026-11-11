//! Bit-exact safe port of OpenMalaria's modified Orson Peters `ChaCha<R>` generator (`contrib/chacha.h`).

use crate::RngCore;
use serde::{Deserialize, Serialize};

const CHACHA_CONSTANTS: [u32; 4] = [0x6170_7865, 0x3320_646e, 0x7962_2d32, 0x6b20_6574];
const OM_SENTINEL: u32 = 0xdead_beef;
const OM_IV_0: u32 = 0xac4f_d2ff;
const OM_IV_1: u32 = 0x1b48_daba;
const DOUBLE_SCALE: f64 = 1.110_223_024_625_156_5e-16; // 2^-53

/// Modified `ChaCha<R>` counter-mode generator with OpenMalaria's custom 10-word key setup.
///
/// # Examples
/// ```
/// use om_rng::{OmChaCha, RngCore};
/// let mut rng = OmChaCha::<8>::new(0, 0);
/// assert_eq!(rng.gen_u32(), 0x3cd0_863f);
/// ```
#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct OmChaCha<const R: usize> {
    block: [u32; 16],
    keysetup: [u32; 10],
    ctr: u64,
}

impl<const R: usize> PartialEq for OmChaCha<R> {
    fn eq(&self, other: &Self) -> bool {
        self.keysetup == other.keysetup && self.ctr == other.ctr
    }
}

impl<const R: usize> Eq for OmChaCha<R> {}

impl<const R: usize> OmChaCha<R> {
    /// Seeds a `ChaCha<R>` generator with a 64-bit seed, 64-bit stream, and OpenMalaria's IV.
    #[must_use]
    pub fn new(seedval: u64, stream: u64) -> Self {
        let mut rng = Self {
            block: [0; 16],
            keysetup: [0; 10],
            ctr: 0,
        };
        rng.seed(seedval, stream);
        rng
    }

    /// Constructs a `ChaCha<R>` generator directly from raw 10-word `keysetup` words (e.g. RFC vectors).
    #[must_use]
    pub fn from_keysetup(keysetup: [u32; 10]) -> Self {
        Self {
            block: [0; 16],
            keysetup,
            ctr: 0,
        }
    }

    /// Re-seeds the generator using OpenMalaria's `seedval` + `stream` + `0xac4fd2ff / 0x1b48daba` IV.
    pub fn seed(&mut self, seedval: u64, stream: u64) {
        self.ctr = 0;
        self.keysetup[0] = (seedval & 0xffff_ffff) as u32;
        self.keysetup[1] = (seedval >> 32) as u32;
        self.keysetup[2] = OM_SENTINEL;
        self.keysetup[3] = OM_SENTINEL;
        self.keysetup[4] = (stream & 0xffff_ffff) as u32;
        self.keysetup[5] = (stream >> 32) as u32;
        self.keysetup[6] = OM_SENTINEL;
        self.keysetup[7] = OM_SENTINEL;
        self.keysetup[8] = OM_IV_0;
        self.keysetup[9] = OM_IV_1;
    }

    /// Advances the internal 32-bit word counter by `n` steps without materializing intermediate blocks.
    pub fn discard(&mut self, n: u64) {
        if n == 0 {
            return;
        }
        let was_zero = self.ctr == 0;
        let idx = self.ctr % 16;
        self.ctr = self.ctr.wrapping_add(n);
        if (was_zero || idx.saturating_add(n) >= 16) && !self.ctr.is_multiple_of(16) {
            self.generate_block();
        }
    }

    /// Returns the current 64-bit word counter (for legacy binary checkpoint compatibility).
    #[must_use]
    pub const fn counter(&self) -> u64 {
        self.ctr
    }

    /// Restores the 64-bit word counter on an already-seeded generator, regenerating the active block if needed.
    pub fn restore_counter(&mut self, ctr: u64) {
        self.ctr = ctr;
        if !self.ctr.is_multiple_of(16) {
            self.generate_block();
        }
    }

    #[inline]
    fn quarter_round(x: &mut [u32; 16], a: usize, b: usize, c: usize, d: usize) {
        x[a] = x[a].wrapping_add(x[b]);
        x[d] ^= x[a];
        x[d] = x[d].rotate_left(16);

        x[c] = x[c].wrapping_add(x[d]);
        x[b] ^= x[c];
        x[b] = x[b].rotate_left(12);

        x[a] = x[a].wrapping_add(x[b]);
        x[d] ^= x[a];
        x[d] = x[d].rotate_left(8);

        x[c] = x[c].wrapping_add(x[d]);
        x[b] ^= x[c];
        x[b] = x[b].rotate_left(7);
    }

    fn generate_block(&mut self) {
        let mut input = [0u32; 16];
        input[0..4].copy_from_slice(&CHACHA_CONSTANTS);
        input[4..12].copy_from_slice(&self.keysetup[0..8]);
        let blk_idx = self.ctr / 16;
        input[12] = (blk_idx & 0xffff_ffff) as u32;
        input[13] = (blk_idx >> 32) as u32;
        input[14] = self.keysetup[8];
        input[15] = self.keysetup[9];

        self.block = input;
        let mut round = 0;
        while round < R {
            Self::quarter_round(&mut self.block, 0, 4, 8, 12);
            Self::quarter_round(&mut self.block, 1, 5, 9, 13);
            Self::quarter_round(&mut self.block, 2, 6, 10, 14);
            Self::quarter_round(&mut self.block, 3, 7, 11, 15);
            Self::quarter_round(&mut self.block, 0, 5, 10, 15);
            Self::quarter_round(&mut self.block, 1, 6, 11, 12);
            Self::quarter_round(&mut self.block, 2, 7, 8, 13);
            Self::quarter_round(&mut self.block, 3, 4, 9, 14);
            round += 2;
        }
        for (b, &inp) in self.block.iter_mut().zip(&input) {
            *b = b.wrapping_add(inp);
        }
    }
}

impl<const R: usize> RngCore for OmChaCha<R> {
    #[inline]
    fn gen_u32(&mut self) -> u32 {
        let idx = (self.ctr % 16) as usize;
        if idx == 0 {
            self.generate_block();
        }
        self.ctr = self.ctr.wrapping_add(1);
        self.block[idx]
    }

    #[inline]
    fn gen_u64(&mut self) -> u64 {
        let low = u64::from(self.gen_u32());
        let high = u64::from(self.gen_u32());
        (high << 32) | low
    }

    #[inline]
    fn gen_double(&mut self) -> f64 {
        let x = u64::from(self.gen_u32());
        ((x << 21) as f64) * DOUBLE_SCALE
    }
}
