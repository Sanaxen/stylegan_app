.. include:: defs.rst

.. cpp:namespace:: zfp

FAQ
===

The following is a list of answers to frequently asked questions.  For
questions not answered here or elsewhere in the documentation, please
`e-mail us <mailto:zfp@llnl.gov>`__.

Questions answered in this FAQ:

  #. :ref:`Can zfp compress vector fields? <q-vfields>`
  #. :ref:`Should I declare a 2D array as zfp::array1d a(nx * ny, rate)? <q-array2d>`
  #. :ref:`How can I initialize a zfp compressed array from disk? <q-read>`
  #. :ref:`Can I use zfp to represent dense linear algebra matrices? <q-matrix>`
  #. :ref:`Can zfp compress logically regular but geometrically irregular data? <q-structured>`
  #. :ref:`Does zfp handle infinities, NaNs,and subnormal floating-point numbers? <q-valid>`
  #. :ref:`Can zfp handle data with some missing values? <q-missing>`
  #. :ref:`Can I use zfp to store integer data? <q-integer>`
  #. :ref:`Can I compress 32-bit integers using zfp? <q-int32>`
  #. :ref:`Why does zfp corrupt memory if my allocated buffer is too small? <q-overrun>`
  #. :ref:`Are zfp compressed streams portable across platforms? <q-portability>`
  #. :ref:`How can I achieve finer rate granularity? <q-granularity>`
  #. :ref:`Can I generate progressive zfp streams? <q-progressive>`
  #. :ref:`How do I initialize the decompressor? <q-init>`
  #. :ref:`Must I use the same parameters during compression and decompression? <q-same>`
  #. :ref:`Do strides have to match during compression and decompression? <q-strides>`
  #. :ref:`Why does zfp sometimes not respect my error tolerance? <q-tolerance>`
  #. :ref:`Why is the actual rate sometimes not what I requested? <q-rate>`
  #. :ref:`Can zfp perform compression in place? <q-inplace>`
  #. :ref:`How should I set the precision to bound the relative error? <q-relerr>`
  #. :ref:`Does zfp support lossless compression? <q-lossless>`
  #. :ref:`Why is my actual, measured error so much smaller than the tolerance? <q-abserr>`
  #. :ref:`Are parallel compressed streams identical to serial streams? <q-parallel>`
  #. :ref:`Are zfp arrays and other data structures thread-safe? <q-thread-safety>`
  #. :ref:`Why does parallel compression performance not match my expectations? <q-omp-perf>`
  #. :ref:`Why are compressed arrays so slow? <q-1d-speed>`
  #. :ref:`Do compressed arrays use reference counting? <q-ref-count>`

-------------------------------------------------------------------------------

.. _q-vfields:

Q1: *Can zfp compress vector fields?*

I have a 2D vector field
::

  double velocity[ny][nx][2];

of dimensions *nx* |times| *ny*.  Can I use a 3D |zfp| array to store this as::

  array3d velocity(2, nx, ny, rate);

A: Although this could be done, zfp assumes that consecutive values are
related.  The two velocity components (*vx*, *vy*) are almost suredly
independent and would not be correlated.  This will severely hurt the
compression rate or quality.  Instead, consider storing *vx* and *vy* as
two separate 2D scalar arrays::

  array2d vx(nx, ny, rate);
  array2d vy(nx, ny, rate);

or as
::

  array2d velocity[2] = {array2d(nx, ny, rate), array2d(nx, ny, rate)};

-------------------------------------------------------------------------------

.. _q-array2d:

Q2: *Should I declare a 2D array as zfp::array1d a(nx * ny, rate)?*

I have a 2D scalar field of dimensions *nx* |times| *ny* that I allocate as
::

  double* a = new double[nx * ny];

and index as
::

  a[x + nx * y]

Should I use a corresponding zfp array
::

  array1d a(nx * ny, rate);

to store my data in compressed form?

A: Although this is certainly possible, if the scalar field exhibits
coherence in both spatial dimensions, then far better results can be
achieved by using a 2D array::

  array2d a(nx, ny, rate);

Although both compressed arrays can be indexed as above, the 2D array can
exploit smoothness in both dimensions and improve the quality dramatically
for the same rate.

Since |zfp| 0.5.2, proxy pointers are also available that act much like
the flat :code:`double*`.

-------------------------------------------------------------------------------

.. _q-read:

Q3: *How can I initialize a zfp compressed array from disk?*

I have a large, uncompressed, 3D data set::

  double a[nz][ny][nx];

stored on disk that I would like to read into a compressed array.  This data
set will not fit in memory uncompressed.  What is the best way of doing this?

A: Using a |zfp| array::

  array3d a(nx, ny, nz, rate);

the most straightforward (but perhaps not best) way is to read one
floating-point value at a time and copy it into the array::

  for (uint z = 0; z < nz; z++)
    for (uint y = 0; y < ny; y++)
      for (uint x = 0; x < nx; x++) {
        double f;
        if (fread(&f, sizeof(f), 1, file) == 1)
          a(x, y, z) = f;
        else {
          // handle I/O error
        }
      }

Note, however, that if the array cache is not large enough, then this may
compress blocks before they have been completely filled.  Therefore it is
recommended that the cache holds at least one complete layer of blocks,
i.e., (*nx* / 4) |times| (*ny* / 4) blocks in the example above.

To avoid inadvertent evictions of partially initialized blocks, it is better
to buffer four layers of *nx* |times| *ny* values each at a time, when
practical, and to completely initialize one block after another, which is
facilitated using |zfp|'s iterators::

  double* buffer = new double[nx * ny * 4];
  int zmin = -4;
  for (zfp::array3d::iterator it = a.begin(); it != a.end(); it++) {
    int x = it.i();
    int y = it.j();
    int z = it.k();
    if (z > zmin + 3) {
      // read another layer of blocks
      if (fread(buffer, sizeof(*buffer), nx * ny * 4, file) != nx * ny * 4) {
        // handle I/O error
      }
      zmin += 4;
    }
    a(x, y, z) = buffer[x + nx * (y + ny * (z - zmin))];
  }

Iterators have been available since |zfp| 0.5.2.

-------------------------------------------------------------------------------

.. _q-matrix:

Q4: *Can I use zfp to represent dense linear algebra matrices?*

A: Yes, but your mileage may vary.  Dense matrices, unlike smooth scalar
fields, rarely exhibit correlation between adjacent rows and columns.  Thus,
the quality or compression ratio may suffer.

-------------------------------------------------------------------------------

.. _q-structured:

Q5: *Can zfp compress logically regular but geometrically irregular data?*

My data is logically structured but irregularly sampled, e.g., it is
rectilinear, curvilinear, or Lagrangian, or uses an irregular spacing of
quadrature points.  Can I still use zfp to compress it?

A: Yes, as long as the data is (or can be) represented as a logical
multidimensional array, though your mileage may vary.  |zfp| has been designed
for uniformly sampled data, and compression will in general suffer the more
irregular the sampling is.

-------------------------------------------------------------------------------

.. _q-valid:

Q6: *Does zfp handle infinities, NaNs,and subnormal floating-point numbers?*

A: Yes, but only in :ref:`reversible mode <mode-reversible>`.

|zfp|'s lossy compression modes currently support only finite
floating-point values.  If a block contains a NaN or an infinity, undefined
behavior is invoked due to the C math function :c:func:`frexp` being
undefined for non-numbers.  Subnormal numbers are, however, handled correctly.

-------------------------------------------------------------------------------

.. _q-missing:

Q7: *Can zfp handle data with some missing values?*

My data has some missing values that are flagged by very large numbers, e.g.
1e30.  Is that OK?

A: Although all finite numbers are "correctly" handled, such large sentinel
values are likely to pollute nearby values, because all values within a block
are expressed with respect to a common largest exponent.  The presence of
very large values may result in complete loss of precision of nearby, valid
numbers.  Currently no solution to this problem is available, but future
versions of |zfp| will likely support a bit mask to tag values that should be
excluded from compression.

-------------------------------------------------------------------------------

.. _q-integer:

Q8: *Can I use zfp to store integer data?*

Can I use zfp to store integer data such as 8-bit quantized images or 16-bit
digital elevation models?

A: Yes (as of version 0.4.0), but the data has to be promoted to 32-bit signed
integers first.  This should be done one block at a time using an appropriate
:c:func:`zfp_promote_*_to_int32` function call (see :file:`zfp.h`).  Future
versions of |zfp| may provide a high-level interface that automatically
performs promotion and demotion.

Note that the promotion functions shift the low-precision integers into the
most significant bits of 31-bit (not 32-bit) integers and also convert unsigned
to signed integers.  Do use these functions rather than simply casting 8-bit
integers to 32 bits to avoid wasting compressed bits to encode leading zeros.
Moreover, in fixed-precision mode, set the precision relative to the precision
of the (unpromoted) source data.

As of version 0.5.1, integer data is supported both by the low-level API and
high-level calls :c:func:`zfp_compress` and :c:func:`zfp_decompress`.

-------------------------------------------------------------------------------

.. _q-int32:

Q9: *Can I compress 32-bit integers using zfp?*

I have some 32-bit integer data.  Can I compress it using |zfp|'s 32-bit
integer support?

A: Yes, this can safely be done in :ref:`reversible mode <mode-reversible>`.

In other (lossy) modes, the answer depends.
|zfp| compression of 32-bit and 64-bit integers requires that each
integer *f* have magnitude \|\ *f*\ \| < 2\ :sup:`30` and
\|\ *f*\ \| < 2\ :sup:`62`, respectively.  To handle signed integers that
span the entire range |minus|\ 2\ :sup:`31` |leq| x < 2\ :sup:`31`, or
unsigned integers 0 |leq| *x* < 2\ :sup:`32`, the data has to be promoted to
64 bits first.

As with floating-point data, the integers should ideally represent a
quantized continuous function rather than, say, categorical data or set of
indices.  Depending on compression settings and data range, the integers may
or may not be losslessly compressed.  If fixed-precision mode is used, the
integers may be stored at less precision than requested.
See :ref:`Q21 <q-lossless>` for more details on precision and lossless
compression.

-------------------------------------------------------------------------------

.. _q-overrun:

Q10: *Why does zfp corrupt memory if my allocated buffer is too small?*

Why does |zfp| corrupt memory rather than return an error code if not enough
memory is allocated for the compressed data?

A: This is for performance reasons.  |zfp| was primarily designed for fast
random access to fixed-rate compressed arrays, where checking for buffer
overruns is unnecessary.  Adding a test for every compressed byte output
would significantly compromise performance.

One way around this problem (when not in fixed-rate mode) is to use the
:c:data:`maxbits` parameter in conjunction with the maximum precision or
maximum absolute error parameters to limit the size of compressed blocks.
Finally, the function :c:func:`zfp_stream_maximum_size` returns a conservative
buffer size that is guaranteed to be large enough to hold the compressed data
and the optional header.

-------------------------------------------------------------------------------

.. index::
   single: Endianness
.. _q-portability:

Q11: *Are zfp compressed streams portable across platforms?*

Are |zfp| compressed streams portable across platforms?  Are there, for
example, endianness issues?

A: Yes, |zfp| can write portable compressed streams.  To ensure portability
across different endian platforms, the bit stream must however be written
in increments of single bytes on big endian processors (e.g., PowerPC, SPARC),
which is achieved by compiling |zfp| with an 8-bit (single-byte) word size::

  -DBIT_STREAM_WORD_TYPE=uint8

See :c:macro:`BIT_STREAM_WORD_TYPE`.  Note that on little endian processors
(e.g., Intel x86-64 and AMD64), the word size does not affect the bit stream
produced, and thus the default word size may be used.  By default, |zfp| uses
a word size of 64 bits, which results in the coarsest rate granularity but
fastest (de)compression.  If cross-platform portability is not needed, then
the maximum word size is recommended (but see also :ref:`Q12 <q-granularity>`).

When using 8-bit words, |zfp| produces a compressed stream that is byte order
independent, i.e., the exact same compressed sequence of bytes is generated
on little and big endian platforms.  When decompressing such streams,
floating-point and integer values are recovered in the native byte order of
the machine performing decompression.  The decompressed values can be used
immediately without the need for byte swapping and without having to worry
about the byte order of the computer that generated the compressed stream.

Finally, |zfp| assumes that the floating-point format conforms to IEEE 754.
Issues may arise on architectures that do not support IEEE floating point.

-------------------------------------------------------------------------------

.. _q-granularity:

Q12: *How can I achieve finer rate granularity?*

A: For *d*-dimensional arrays, |zfp| supports a rate granularity of 8 / |4powd|
bits, i.e., the rate can be specified in increments of a fraction of a bit for
2D and 3D arrays.  Such fine rate selection is always available for sequential
compression (e.g., when calling :c:func:`zfp_compress`).

Unlike in sequential compression, |zfp|'s compressed arrays require random
access writes, which are supported only at the granularity of whole words.
By default, a word is 64 bits, which gives a rate granularity of
64 / |4powd| in *d* dimensions, i.e., 16 bits in 1D, 4 bits in 2D, and 1 bit
in 3D.

To achieve finer granularity, recompile |zfp| with a smaller (but as large as
possible) stream word size, e.g.::

  -DBIT_STREAM_WORD_TYPE=uint8

gives the finest possible granularity, but at the expense of (de)compression
speed.  See :c:macro:`BIT_STREAM_WORD_TYPE`.

-------------------------------------------------------------------------------

.. _q-progressive:

Q13: *Can I generate progressive zfp streams?*

A: Yes, but it requires some coding effort.  There is currently no high-level
support for progressive |zfp| streams.  To implement progressive fixed-rate
streams, the fixed-length bit streams should be interleaved among the blocks
that make up an array.  For instance, if a 3D array uses 1024 bits per block,
then those 1024 bits could be broken down into, say, 16 pieces of 64 bits
each, resulting in 16 discrete quality settings.  By storing the blocks
interleaved such that the first 64 bits of all blocks are contiguous,
followed by the next 64 bits of all blocks, etc., one can achieve progressive
decompression by setting the :c:member:`zfp_stream.maxbits` parameter (see
:c:func:`zfp_stream_set_params`) to the number of bits per block received so
far.

To enable interleaving of blocks, |zfp| must first be compiled with::

  -DBIT_STREAM_STRIDED

to enable strided bit stream access.  In the example above, if the stream
word size is 64 bits and there are *n* blocks, then::

  stream_set_stride(stream, m, n);

implies that after every *m* 64-bit words have been decoded, the bit stream
is advanced by *m* |times| *n* words to the next set of m 64-bit words
associated with the block.

-------------------------------------------------------------------------------

.. _q-init:

Q14: *How do I initialize the decompressor?*

A: The :c:type:`zfp_stream` and :c:type:`zfp_field` objects usually need to
be initialized with the same values as they had during compression (but see
:ref:`Q15 <q-same>` for exceptions).
These objects hold the compression mode and parameters, and field data like
the scalar type and dimensions.  By default, these parameters are not stored
with the compressed stream (the "codestream") and prior to |zfp| 0.5.0 had to
be maintained separately by the application.

Since version 0.5.0, functions exist for reading and writing a 12- to 19-byte
header that encodes compression and field parameters.  For applications that
wish to embed only the compression parameters, e.g., when the field dimensions
are already known, there are separate functions that encode and decode this
information independently.

-------------------------------------------------------------------------------

.. _q-same:

Q15: *Must I use the same parameters during compression and decompression?*

A: Not necessarily.  When decompressing one block at a time, it is possible
to use more tightly constrained :c:type:`zfp_stream` parameters during
decompression than were used during compression.  For instance, one may use a
larger :c:member:`zfp_stream.minbits`, smaller :c:member:`zfp_stream.maxbits`,
smaller :c:member:`zfp_stream.maxprec`, or larger :c:member:`zfp_stream.minexp`
during decompression to process fewer compressed bits than are stored, and to
decompress the array more quickly at a lower precision.  This may be useful
in situations where the precision and accuracy requirements are not known a
priori, thus forcing conservative settings during compression, or when the
compressed stream is used for multiple purposes.  For instance, visualization
usually has less stringent precision requirements than quantitative data
analysis.  This feature of decompressing to a lower precision is particularly
useful when the stream is stored progressively (see :ref:`Q13 <q-progressive>`).

Note that one may not use less constrained parameters during decompression,
e.g., one cannot ask for more than :c:member:`zfp_stream.maxprec` bits of
precision when decompressing.  Furthermore, the parameters must agree between
compression and decompression when calling the high-level API function
:c:func:`zfp_decompress`.

Currently float arrays have a different compressed representation from
compressed double arrays due to differences in exponent width.  It is not
possible to compress a double array and then decompress (demote) the result
to floats, for instance.  Future versions of the |zfp| codec may use a unified
representation that does allow this.

-------------------------------------------------------------------------------

.. _q-strides:

Q16: *Do strides have to match during compression and decompression?*

A: No.  For instance, a 2D vector field::

  float in[ny][nx][2];

could be compressed as two scalar fields with strides *sx* = 2,
*sy* = 2 |times| *nx*, and with pointers :code:`&in[0][0][0]` and
:code:`&in[0][0][1]` to the first value of each scalar field.  These two
scalar fields can later be decompressed as non-interleaved fields::

  float out[2][ny][nx];

using strides *sx* = 1, *sy* = *nx* and pointers :code:`&out[0][0][0]`
and :code:`&out[1][0][0]`.

-------------------------------------------------------------------------------

.. _q-tolerance:

Q17: *Why does zfp sometimes not respect my error tolerance?*

A: First, |zfp| does not support
:ref:`fixed-accuracy mode <mode-fixed-accuracy>` for integer data and
will ignore any tolerance requested via :c:func:`zfp_stream_set_accuracy`
or associated :ref:`expert mode <mode-expert>` parameter settings.

For floating-point data, |zfp| does not store each scalar value independently
but represents a group of values (4, 16, 64, or 256 values, depending on
dimensionality) as linear combinations like averages by evaluating arithmetic
expressions.  Just like in uncompressed IEEE floating-point arithmetic, both
representation error and roundoff error in the least significant bit(s) often
occur.

To illustrate this, consider compressing the following 1D array of four
floats::

  float f[4] = { 1, 1e-1, 1e-2, 1e-3 };

using the |zfp| command-line tool::

  zfp -f -1 4 -a 0 -i input.dat -o output.dat

In spite of an error tolerance of zero, the reconstructed values are::

  float g[4] = { 1, 1e-1, 9.999998e-03, 9.999946e-04 };

with a (computed) maximum error of 5.472e-9.  Because f[3] = 1e-3 can only
be approximately represented in radix-2 floating-point, the actual error
is even smaller: 5.424e-9.  This reconstruction error is primarily due to
|zfp|'s block-floating-point representation, which expresses the four values
in a block relative to a single, common binary exponent.  Such exponent
alignment occurs also in regular IEEE floating-point operations like addition.
For instance,::

  float x = (f[0] + f[3]) - 1;

should of course result in :code:`x = f[3] = 1e-3`, but due to exponent
alignment a few of the least significant bits of f[3] are lost in the
addition, giving a result of :code:`x = 1.0000467e-3` and a roundoff error
of 4.668e-8.  Similarly,::

  float sum = f[0] + f[1] + f[2] + f[3];

should return :code:`sum = 1.111`, but is computed as 1.1110000610.  Moreover,
the value 1.111 cannot even be represented exactly in (radix-2) floating-point;
the closest float is 1.1109999.  Thus the computed error::

  float error = sum - 1.111f;

which itself has some roundoff error, is 1.192e-7.

*Phew*!  Note how the error introduced by |zfp| (5.472e-9) is in fact one to
two orders of magnitude smaller than the roundoff errors (4.668e-8 and
1.192e-7) introduced by IEEE floating-point in these computations.  This lower
error is in part due to |zfp|'s use of 30-bit significands compared to IEEE's
24-bit single-precision significands.  Note that data sets with a large dynamic
range, e.g., where adjacent values differ a lot in magnitude, are more
susceptible to representation errors.

The moral of the story is that error tolerances smaller than machine epsilon
(relative to the data range) cannot always be satisfied by |zfp|.  Nor are such
tolerances necessarily meaningful for representing floating-point data that
originated in floating-point arithmetic expressions, since accumulated
roundoff errors are likely to swamp compression errors.  Because such roundoff
errors occur frequently in floating-point arithmetic, insisting on lossless
compression on the grounds of accuracy is tenuous at best.

-------------------------------------------------------------------------------

.. _q-rate:

Q18: *Why is the actual rate sometimes not what I requested?*

A: In principle, |zfp| allows specifying the size of a compressed block in
increments of single bits, thus allowing very fine-grained tuning of the
bit rate.  There are, however, cases when the desired rate does not exactly
agree with the effective rate, and users are encouraged to check the return
value of :c:func:`zfp_stream_set_rate`, which gives the actual rate.

There are several reasons why the requested rate may not be honored.  First,
the rate is specified in bits/value, while |zfp| always represents a block
of |4powd| values in *d* dimensions, i.e., using
*N* = |4powd| |times| *rate* bits.  *N* must be an integer number of bits,
which constrains the actual rate to be a multiple of 1 / |4powd|.  The actual
rate is computed by rounding |4powd| times the desired rate.

Second, if the array dimensions are not multiples of four, then |zfp| pads the
dimensions to the next higher multiple of four.  Thus, the total number of
bits for a 2D array of dimensions *nx* |times| *ny* is computed in terms of
the number of blocks *bx* |times| *by*::

  bitsize = (4 * bx) * (4 * by) * rate

where *nx* |leq| 4 |times| bx < *nx* + 4 and
*ny* |leq| 4 |times| *by* < *ny* + 4.  When amortizing bitsize over the
*nx* |times| *ny* values, a slightly higher rate than requested may result.

Third, to support updating compressed blocks, as is needed by |zfp|'s
compressed array classes, the user may request write random access to the
fixed-rate stream.  To support this, each block must be aligned on a stream
word boundary (see :ref:`Q12 <q-granularity>`), and therefore the rate when
write random access is requested must be a multiple of *wordsize* / |4powd|
bits.  By default *wordsize* = 64 bits.

Fourth, for floating-point data, each block must hold at least the common
exponent and one additional bit, which places a lower bound on the rate.

Finally, the user may optionally include a header with each array.  Although
the header is small, it must be accounted for in the rate.  The function
:c:func:`zfp_stream_maximum_size` conservatively includes space for a header,
for instance.

-------------------------------------------------------------------------------

.. _q-inplace:

Q19: *Can zfp perform compression in place?*

A: Because the compressed data tends to be far smaller than the uncompressed
data, it is natural to ask if the compressed stream can overwrite the
uncompressed array to avoid having to allocate separate storage for the
compressed stream.  |zfp| does allow for the possibility of such in-place
compression, but with several caveats and restrictions:

  1. A bitstream must be created whose buffer points to the beginning of
     uncompressed (and to be compressed) storage.

  2. The array must be compressed using |zfp|'s low-level API.  In particular,
     the data must already be partitioned and organized into contiguous blocks
     so that all values of a block can be pulled out once and then replaced
     with the corresponding shorter compressed representation.

  3. No one compressed block can occupy more space than its corresponding
     uncompressed block so that the not-yet compressed data is not overwritten.
     This is usually easily accomplished in fixed-rate mode, although the
     expert interface also allows guarding against this in all modes using the
     :c:member:`zfp_stream.maxbits` parameter.  This parameter should be set to
     :code:`maxbits = 4^d * 8 * sizeof(type)`, where *d* is the array
     dimensionality (1, 2, or 3) and where *type* is the scalar type of the
     uncompressed data.

  4. No header information may be stored in the compressed stream.

In-place decompression can also be achieved, but in addition to the above
constraints requires even more care:

  1. The data must be decompressed in reverse block order, so that the last
     block is decompressed first to the end of the block array.  This requires
     the user to maintain a pointer to uncompressed storage and to seek via
     :c:func:`stream_rseek` to the proper location in the compressed stream
     where the block is stored.

  2. The space allocated to the compressed stream must be large enough to
     also hold the uncompressed data.

An :ref:`example <ex-inplace>` is provided that shows how in-place compression
can be done.

-------------------------------------------------------------------------------

.. _q-relerr:

Q20: *How should I set the precision to bound the relative error?*

A: In general, |zfp| cannot bound the point-wise relative error due to its
use of a block-floating-point representation, in which all values within a
block are represented in relation to a single common exponent.  For a high
enough dynamic range within a block there may simply not be enough precision
available to guard against loss.  For instance, a block containing the values
2\ :sup:`0` = 1 and 2\ :sup:`-n` would require a precision of *n* + 3 bits to
represent losslessly, and |zfp| uses at most 64-bit integers to represent
values.  Thus, if *n* |geq| 62, then 2\ :sup:`-n` is replaced with 0, which
is a 100% relative error.  Note that such loss also occurs when, for instance,
2\ :sup:`0` and 2\ :sup:`-n` are added using floating-point arithmetic (see
also :ref:`Q17 <q-tolerance>`).

It is, however, possible to bound the error relative to the largest (in
magnitude) value, *fmax*, within a block, which if the magnitude of values
does not change too rapidly may serve as a reasonable proxy for point-wise
relative errors.

One might then ask if using |zfp|'s fixed-precision mode with *p* bits of
precision ensures that the block-wise relative error is at most
2\ :sup:`-p` |times| *fmax*.  This is, unfortunately, not the case, because
the requested precision, *p*, is ensured only for the transform coefficients.
During the inverse transform of these quantized coefficients the quantization
error may amplify.  That being said, it is possible to derive a bound on the
error in terms of *p* that would allow choosing an appropriate precision.
Such a bound is derived below.

Let
::

  emax = floor(log2(fmax))

be the largest base-2 exponent within a block.  For transform coefficient
precision, *p*, one can show that the maximum absolute error, *err*, is
bounded by::

  err <= k(d) * (2^emax / 2^p) <= k(d) * (fmax / 2^p)

Here *k*\ (*d*) is a constant that depends on the data dimensionality *d*::

  k(d) = 20 * (15/4)^(d-1)

so that in 1D, 2D, 3D, and 4D we have::

  k(1) = 20
  k(2) = 125
  k(3) = 1125/4
  k(4) = 16876/16

Thus, to guarantee *n* bits of accuracy in the decompressed data, we need
to choose a higher precision, *p*, for the transform coefficients::

  p(n, d) = n + ceil(log2(k(d))) = n + 2 * d + 3

so that
::

  p(n, 1) = n + 5
  p(n, 2) = n + 7
  p(n, 3) = n + 9
  p(n, 4) = n + 11

This *p* value should be used in the call to
:c:func:`zfp_stream_set_precision`.

Note, again, that some values in the block may have leading zeros when
expressed relative to 2\ :sup:`emax`, and these leading zeros are counted
toward the *n*-bit precision.  Using decimal to illustrate this, suppose
we used 4-digit precision for a 1D block containing these four values::

  -1.41421e+1 ~ -1.414e+1 = -1414 * (10^1 / 1000)
  +2.71828e-1 ~ +0.027e+1 =   +27 * (10^1 / 1000)
  +3.14159e-6 ~ +0.000e+1 =     0 * (10^1 / 1000)
  +1.00000e+0 ~ +0.100e+1 =  +100 * (10^1 / 1000)

with the values in the middle column aligned to the common base-10 exponent
+1, and with the values on the right expressed as scaled integers.  These
are all represented using four digits of precision, but some of those digits
are leading zeros.

-------------------------------------------------------------------------------

.. _q-lossless:

Q21: *Does zfp support lossless compression?*

A: Yes.  As of |zfp| |revrelease|, bit-for-bit lossless compression is
supported via the :ref:`reversible compression mode <mode-reversible>`.
This mode supports both integer and floating-point data.

In addition, it is sometimes possible to ensure lossless compression using
|zfp|'s fixed-precision and fixed-accuracy modes.  For integer data, |zfp|
can with few exceptions ensure lossless compression in
:ref:`fixed-precision mode <mode-fixed-precision>`.
For a given *n*-bit integer type (*n* = 32 or *n* = 64), consider compressing
*p*-bit signed integer data, with the sign bit counting toward the precision.
In other words, there are exactly 2\ :sup:`p` possible signed integers.  If
the integers are unsigned, then subtract 2\ :sup:`p-1` first so that they
range from |minus|\ 2\ :sup:`p-1` to 2\ :sup:`p-1` - 1.

Lossless integer compression in fixed-precision mode is achieved by first
promoting the *p*-bit integers to *n* - 1 bits (see :ref:`Q8 <q-integer>`)
such that all integer values fall in
[|minus|\ 2\ :sup:`30`, +2\ :sup:`30`), when *n* = 32, or in
[|minus|\ 2\ :sup:`62`, +2\ :sup:`62`), when *n* = 64.  In other words, the
*p*-bit integers first need to be shifted left by *n* - *p* - 1 bits.  After
promotion, the data should be compressed in zfp's fixed-precision mode using::

  q = p + 4 * d + 1

bits of precision to ensure no loss, where *d* is the data dimensionality
(1 |leq| d |leq| 4).  Consequently, the *p*-bit data can be losslessly
compressed as long as *p* |leq| *n* - 4 |times| *d* - 1.  The table below
lists the maximum precision *p* that can be losslessly compressed using 32-
and 64-bit integer types.

  = ==== ====
  d n=32 n=64
  = ==== ====
  1 27   59
  2 23   55
  3 19   51
  4 15   47
  = ==== ====

Although lossless compression is possible as long as the precision constraint
is met, the precision needed to guarantee no loss is generally much higher
than the precision intrinsic in the uncompressed data.  Therefore, we
recommend using the :ref:`reversible mode <mode-reversible>` when lossless
compression is desired.

The minimum precision, *q*, given above is often larger than what
is necessary in practice.  There are worst-case inputs that do require such
large *q* values, but they are quite rare.

The reason for expanded precision, i.e., why *q* > *p*, is that |zfp|'s
decorrelating transform computes averages of integers, and this transform is
applied *d* times in *d* dimensions.  Each average of two *p*-bit numbers
requires *p* + 1 bits to avoid loss, and each transform can be thought of
involving up to four such averaging operations.

For floating-point data, fully lossless compression with |zfp| usually
requires :ref:`reversible mode <mode-reversible>`, as the other compression
modes are unlikely to guarantee bit-for-bit exact reconstructions.  However,
if the dynamic range is low or varies slowly such that values
within a |4powd| block have the same or similar exponent, then the
precision gained by discarding the 8 or 11 bits of the common floating-point
exponents can offset the precision lost in the decorrelating transform.  For
instance, if all values in a block have the same exponent, then lossless
compression is obtained using
*q* = 26 + 4 |times| *d* |leq| 32 bits of precision for single-precision data
and *q* = 55 + 4 |times| *d* |leq| 64 bits of precision for double-precision
data.  Of course, the constraint imposed by the available integer precision
*n* implies that lossless compression of such data is possible only in 1D for
single-precision data and only in 1D and 2D for double-precision data.
Finally, to preserve special values such as negative zero, plus and minues
infinity, and NaNs, reversible mode is needed.

-------------------------------------------------------------------------------

.. _q-abserr:

Q22: *Why is my actual, measured error so much smaller than the tolerance?*

A: For two reasons.  The way |zfp| bounds the absolute error in
:ref:`fixed-accuracy mode <mode-fixed-accuracy>` is by keeping all transform
coefficient bits whose place value exceeds the tolerance while discarding the
less significant bits.  Each such bit has a place value that is a power of
two, and therefore the tolerance must first be rounded down to the next
smaller power of two, which itself will introduce some slack.  This possibly
lower, effective tolerance is returned by the
:c:func:`zfp_stream_set_accuracy` call.

Second, the quantized coefficients are then put through an inverse transform.
This linear transform will combine signed quantization errors that, in the
worst case, may cause them to add up and increase the error, even though the
average (RMS) error remains the same, i.e., some errors cancel while others
compound.  For *d*-dimensional data, *d* such inverse transforms are applied,
with the possibility of errors cascading across transforms.  To account for
the worst possible case, zfp has to conservatively lower its internal error
tolerance further, once for each of the *d* transform passes.

Unless the data is highly oscillatory or noisy, the error is not likely to
be magnified much, leaving an observed error in the decompressed data that
is much lower than the prescribed tolerance.  In practice, the observed
maximum error tends to be about 4-8 times lower than the error tolerance
for 3D data, while the difference is smaller for 2D and 1D data.

We recommend experimenting with tolerances and evaluating what error levels
are appropriate for each application, e.g., by starting with a low,
conservative tolerance and successively doubling it.  The distribution of
errors produced by |zfp| is approximately Gaussian, so even if the maximum
error may seem large at an individual grid point, most errors tend to be
much smaller and tightly clustered around zero.

-------------------------------------------------------------------------------

.. _q-parallel:

Q23: *Are parallel compressed streams identical to serial streams?*

A: Yes, it matters not what execution policy is used; the final compressed
stream produced by :c:func:`zfp_compress` depends only on the uncompressed
data and compression settings.

To support future parallel decompression, in particular variable-rate
streams, it will be necessary to also store an index of where (at what
bit offset) each compressed block is stored in the stream.  Extensions to the
current |zfp| format are being considered to support parallel decompression.

Regardless, the execution policy and parameters such as number of threads
do not need to be the same for compression and decompression.

-------------------------------------------------------------------------------

.. _q-thread-safety:

Q24: *Are zfp's compressed arrays and other data structures thread-safe?*

A: Yes, compressed arrays can be made thread-safe; no, data structures
like :c:type:`zfp_stream` and :c:type:`bitstream` are not necessarily
thread-safe.  As of |zfp| |viewsrelease|, thread-safe read and write access
to compressed arrays is provided through the use of
:ref:`private views <private_immutable_view>`, although these come with
certain restrictions and requirements such as the need for the user to
enforce cache coherence.  Please see the documentation on
:ref:`views <views>` for further details.

As far as C objects, |zfp|'s parallel OpenMP compressor assigns one
:c:type:`zfp_stream` per thread, each of which uses its own private
:c:type:`bitstream`.  Users who wish to make parallel calls to |zfp|'s
:ref:`low-level functions <ll-api>` are advised to consult the source
files :file:`ompcompress.c` and :file:`parallel.c`.

Finally, the |zfp| API is thread-safe as long as multiple threads do not
simultaneously call API functions and pass the same :c:type:`zfp_stream`
or :c:type:`bitstream` object.

-------------------------------------------------------------------------------

.. _q-omp-perf:

Q25: *Why does parallel compression performance not match my expectations?*

A: |zfp| partitions arrays into chunks and assigns each chunk to an OpenMP
thread.  A chunk is a sequence of consecutive *d*-dimensional blocks, each
composed of |4powd| values.  If there are fewer chunks than threads, then
full processor utilization will not be achieved.

The number of chunks is by default set to the number of threads, but can
be modified by the user via :c:func:`zfp_stream_set_omp_chunk_size`.
One reason for using more chunks than threads is to provide for better
load balance.  If compression ratios vary significantly across the array,
then threads that process easy-to-compress blocks may finish well ahead
of threads in charge of difficult-to-compress blocks.  By breaking chunks
into smaller units, OpenMP is given the opportunity to balance the load
better (though the effect of using smaller chunks depends on OpenMP
thread scheduling).  If chunks are too small, however, then the overhead
of allocating and initializing chunks and assigning threads to them may
dominate.  Experimentation with chunk size may improve performance, though
chunks ought to be at least several hundred blocks each.

In variable-rate mode, compressed chunk sizes are not known ahead of time.
Therefore the compressed chunks must be concatenated into a single stream
following compression.  This task is performed sequentially on a single
thread, and will inevitably limit parallel efficiency.

Other reasons for poor parallel performance include compressing arrays
that are too small to offset the overhead of thread creation and
synchronization.  Arrays should ideally consist of thousands of blocks
to offset the overhead of setting up parallel compression.

-------------------------------------------------------------------------------

.. _q-1d-speed:

Q26: *Why are compressed arrays so slow?*

A: This is likely due to the use of a very small cache.  Prior to |zfp|
|csizerelease|, all arrays used two 'layers' of blocks as default cache
size, which is reasonable for 2D and higher-dimensional arrays (as long
as they are not too 'skinny').  In 1D, however, this implies that the
cache holds only two blocks, which is likely to cause excessive thrashing.

As of version |csizerelease|, the default cache size is roughly proportional
to the square root of the total number of array elements, regardless of
array dimensionality.  While this tends to reduce thrashing, we suggest
experimenting with larger cache sizes of at least a few kilobytes to ensure
acceptable performance.

Note that compressed arrays constructed with the
:ref:`default constructor <array_ctor_default>` will
have an initial cache size of only one block.  Therefore, users should call
:cpp:func:`array::set_cache_size` after :ref:`resizing <array_resize>`
such arrays to ensure a large enough cache.

Depending on factors such as rate, cache size, array access pattern,
array access primitive (e.g., indices vs. iterators), and arithmetic
intensity, we usually observe an application slow-down of 1-10x when
switching from uncompressed to compressed arrays.

-------------------------------------------------------------------------------

.. _q-ref-count:

Q27: *Do compressed arrays use reference counting?*

A: It is possible to reference compressed  array elements via proxy
:ref:`references <references>` and :ref:`pointers <pointers>`, through
:ref:`iterators <iterators>`, and through :ref:`views <views>`.  Such
indirect references are valid only during the lifetime of the underlying
array.  No reference counting and garbage collection is used to keep the
array alive if there are external references to it.  Such references
become invalid once the array is destructed, and dereferencing them will
likely lead to segmentation faults.
