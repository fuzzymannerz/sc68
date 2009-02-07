/** Setup function for sc68 original ym engine.
 *
 *    The ym_orig_setup() function sets ym original engine for this ym
 *    emulator instance.
 *
 *  @parm    ym  ym emulator instance to setup
 *  @retval   0  on success
 *  @retval  -1  on failure
 */
int ym_orig_setup(ym_t * const ym);

/** Creates and parse ym original engine options
 *
 *  @params  argc  argument count
 *  @params  argv  argument values
 *  @retval  remaining argument count
 */
int ym_orig_options(int argc, char ** argv);


typedef void (*ym_orig_filter_t)(ym_t * const);

/** YM-2149 internal data structure for original emulator. */
struct ym2149_orig_s
{

  /** @name  Envelop generator
   *  @{
   */
#if YM_ENV_TABLE
  int env_ct;               /**< Envelop period counter                  */
  int env_bit;              /**< Envelop level : 5 LSB are envelop level */
#else
  unsigned int env_ct;      /**< Envelop period counter                  */
  unsigned int env_bit;     /**< Envelop level : 5 LSB are envelop level */
  unsigned int env_cont;    /**< Continue mask [0 or 0x1f]               */
  unsigned int env_alt;     /**< Alternate mask [0 or 0x1f]              */  
  unsigned int env_bitstp;  /**< Envelop level step : [0 or 1]           */
#endif
  s32 * envptr;             /**< generated envelop pointer               */
  /**@}*/

  /** @name  Noise generator
   *  @{
   */
  unsigned int noise_gen;   /**< Noise generator 17-bit shift register   */
  unsigned int noise_ct;    /**< Noise generator period counter          */
  s32 * noiptr;             /**< generated noise pointer                 */
  /**@}*/

  /** @name  Tone generator
   *  @{
   */
  int voice_ctA;            /**< Canal A sound period counter            */
  int voice_ctB;            /**< Canal B sound period counter            */
  int voice_ctC;            /**< Canal C sound period counter            */
  unsigned int levels;      /**< Square level 0xCBA                      */
  s32 * tonptr;             /**< generated tone pointer                  */
  /**@}*/

  /** @name  1-pole filter
   *  @{
   */
  int68_t hipass_inp1;      /**< high pass filter input                  */
  int68_t hipass_out1;      /**< high pass filter output                 */
  int68_t lopass_out1;      /**< low pass filter output                  */
  /**@}*/

  /** 2-poles butterworth filter */
  struct {
    int68_t x[2];
    int68_t y[2];
    int68_t a[3];
    int68_t b[2];
  } btw;

  int ifilter;	            /**< filter function to use.                 */

};

/** YM-2149 emulator instance type */
typedef struct ym2149_orig_s ym_orig_t;

