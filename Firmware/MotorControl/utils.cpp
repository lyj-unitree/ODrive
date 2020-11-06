
#include <utils.hpp>
#include <board.h>


// Compute rising edge timings (0.0 - 1.0) as a function of alpha-beta
// as per the magnitude invariant clarke transform
// The magnitude of the alpha-beta vector may not be larger than sqrt(3)/2
// Returns true on success, and false if the input was out of range
std::tuple<float, float, float, bool> SVM(float alpha, float beta) {
    float tA, tB, tC;
    int Sextant;

    if (beta >= 0.0f) {
        if (alpha >= 0.0f) {
            //quadrant I
            if (one_by_sqrt3 * beta > alpha)
                Sextant = 2; //sextant v2-v3
            else
                Sextant = 1; //sextant v1-v2
        } else {
            //quadrant II
            if (-one_by_sqrt3 * beta > alpha)
                Sextant = 3; //sextant v3-v4
            else
                Sextant = 2; //sextant v2-v3
        }
    } else {
        if (alpha >= 0.0f) {
            //quadrant IV
            if (-one_by_sqrt3 * beta > alpha)
                Sextant = 5; //sextant v5-v6
            else
                Sextant = 6; //sextant v6-v1
        } else {
            //quadrant III
            if (one_by_sqrt3 * beta > alpha)
                Sextant = 4; //sextant v4-v5
            else
                Sextant = 5; //sextant v5-v6
        }
    }

    switch (Sextant) {
        // sextant v1-v2
        case 1: {
            // Vector on-times
            float t1 = alpha - one_by_sqrt3 * beta;
            float t2 = two_by_sqrt3 * beta;

            // PWM timings
            tA = (1.0f - t1 - t2) * 0.5f;
            tB = tA + t1;
            tC = tB + t2;
        } break;

        // sextant v2-v3
        case 2: {
            // Vector on-times
            float t2 = alpha + one_by_sqrt3 * beta;
            float t3 = -alpha + one_by_sqrt3 * beta;

            // PWM timings
            tB = (1.0f - t2 - t3) * 0.5f;
            tA = tB + t3;
            tC = tA + t2;
        } break;

        // sextant v3-v4
        case 3: {
            // Vector on-times
            float t3 = two_by_sqrt3 * beta;
            float t4 = -alpha - one_by_sqrt3 * beta;

            // PWM timings
            tB = (1.0f - t3 - t4) * 0.5f;
            tC = tB + t3;
            tA = tC + t4;
        } break;

        // sextant v4-v5
        case 4: {
            // Vector on-times
            float t4 = -alpha + one_by_sqrt3 * beta;
            float t5 = -two_by_sqrt3 * beta;

            // PWM timings
            tC = (1.0f - t4 - t5) * 0.5f;
            tB = tC + t5;
            tA = tB + t4;
        } break;

        // sextant v5-v6
        case 5: {
            // Vector on-times
            float t5 = -alpha - one_by_sqrt3 * beta;
            float t6 = alpha - one_by_sqrt3 * beta;

            // PWM timings
            tC = (1.0f - t5 - t6) * 0.5f;
            tA = tC + t5;
            tB = tA + t6;
        } break;

        // sextant v6-v1
        case 6: {
            // Vector on-times
            float t6 = -two_by_sqrt3 * beta;
            float t1 = alpha + one_by_sqrt3 * beta;

            // PWM timings
            tA = (1.0f - t6 - t1) * 0.5f;
            tC = tA + t1;
            tB = tC + t6;
        } break;
    }

    bool result_valid =
            tA >= 0.0f && tA <= 1.0f
         && tB >= 0.0f && tB <= 1.0f
         && tC >= 0.0f && tC <= 1.0f;
    return {tA, tB, tC, result_valid};
}

// based on https://math.stackexchange.com/a/1105038/81278
float fast_atan2(float y, float x) {
    // a := min (|x|, |y|) / max (|x|, |y|)
    float abs_y = std::abs(y);
    float abs_x = std::abs(x);
    // inject FLT_MIN in denominator to avoid division by zero
    float a = std::min(abs_x, abs_y) / (std::max(abs_x, abs_y) + std::numeric_limits<float>::min());
    // s := a * a
    float s = a * a;
    // r := ((-0.0464964749 * s + 0.15931422) * s - 0.327622764) * s * a + a
    float r = ((-0.0464964749f * s + 0.15931422f) * s - 0.327622764f) * s * a + a;
    // if |y| > |x| then r := 1.57079637 - r
    if (abs_y > abs_x)
        r = 1.57079637f - r;
    // if x < 0 then r := 3.14159274 - r
    if (x < 0.0f)
        r = 3.14159274f - r;
    // if y < 0 then r := -r
    if (y < 0.0f)
        r = -r;

    return r;
}

// @brief: Returns how much time is left until the deadline is reached.
// If the deadline has already passed, the return value is 0 (except if
// the deadline is very far in the past)
uint32_t deadline_to_timeout(uint32_t deadline_ms) {
    uint32_t now_ms = (uint32_t)((1000ull * (uint64_t)osKernelSysTick()) / osKernelSysTickFrequency);
    uint32_t timeout_ms = deadline_ms - now_ms;
    return (timeout_ms & 0x80000000) ? 0 : timeout_ms;
}

// @brief: Converts a timeout to a deadline based on the current time.
uint32_t timeout_to_deadline(uint32_t timeout_ms) {
    uint32_t now_ms = (uint32_t)((1000ull * (uint64_t)osKernelSysTick()) / osKernelSysTickFrequency);
    return now_ms + timeout_ms;
}

// @brief: Returns a non-zero value if the specified system time (in ms)
// is in the future or 0 otherwise.
// If the time lies far in the past this may falsely return a non-zero value.
int is_in_the_future(uint32_t time_ms) {
    return deadline_to_timeout(time_ms);
}

// @brief: Returns number of microseconds since system startup
uint32_t micros(void) {
    register uint32_t ms, cycle_cnt;
    do {
        ms = HAL_GetTick();
        cycle_cnt = TIM_TIME_BASE->CNT;
     } while (ms != HAL_GetTick());

    return (ms * 1000) + cycle_cnt;
}

// @brief: Busy wait delay for given amount of microseconds (us)
void delay_us(uint32_t us)
{
    uint32_t start = micros();
    while (micros() - start < (uint32_t) us) {
        asm volatile ("nop");
    }
}

lookup_table_t pdf_table = {
    {
        0.3989422804014327f,
        0.3989300581391332f,
        0.3988933935988851f,
        0.3988322935199517f,
        0.39874676913214424f,
        0.3986368361523825f,
        0.39850251477988063f,
        0.39834382968996185f,
        0.3981608100265035f,
        0.39795348939301645f,
        0.3977219058423624f,
        0.39746610186511305f,
        0.3971861243765573f,
        0.39688202470236067f,
        0.3965538585628836f,
        0.3962016860561666f,
        0.3958255716395864f,
        0.39542558411019496f,
        0.3950017965837461f,
        0.3945542864724213f,
        0.3940831354612623f,
        0.3935884294833223f,
        0.3930702586935447f,
        0.3925287174413817f,
        0.3919639042421633f,
        0.39137592174723046f,
        0.3907648767128433f,
        0.3901308799678786f,
        0.38947404638033023f,
        0.38879449482262635f,
        0.38809234813577786f,
        0.38736773309237443f,
        0.3866207803584418f,
        0.3858516244541779f,
        0.3850604037135844f,
        0.3842472602430091f,
        0.38341233987861806f,
        0.3825557921428152f,
        0.38167777019962623f,
        0.3807784308090673f,
        0.3798579342805163f,
        0.37891644442510614f,
        0.377954128507161f,
        0.3769711571946941f,
        0.3759677045089885f,
        0.37494394777328155f,
        0.37390006756057426f,
        0.37283624764058665f,
        0.3717526749258812f,
        0.37064953941717693f,
        0.3695270341478755f,
        0.36838535512782267f,
        0.367224701286328f,
        0.36604527441446577f,
        0.36484727910668047f,
        0.36363092270172065f,
        0.3623964152229252f,
        0.36114396931788534f,
        0.3598738001975069f,
        0.35858612557449837f,
        0.3572811656013071f,
        0.35595914280753055f,
        0.35462028203682544f,
        0.353264810383342f,
        0.35189295712770635f,
        0.35050495367257717f,
        0.3491010334778021f,
        0.3476814319951985f,
        0.3462463866029853f,
        0.34479613653988894f,
        0.3433309228389523f,
        0.3418509882610688f,
        0.34035657722826945f,
        0.33884793575678757f,
        0.3373253113899265f,
        0.33578895313075613f,
        0.3342391113746637f,
        0.33267603784178407f,
        0.33109998550933467f,
        0.3295112085438813f,
        0.32790996223355795f,
        0.3262965029202688f,
        0.3246710879318937f,
        0.32303397551452523f,
        0.32138542476475995f,
        0.3197256955620692f,
        0.3180550485012739f,
        0.3163737448251472f,
        0.31468204635716945f,
        0.3129802154344589f,
        0.31126851484090257f,
        0.3095472077405091f,
        0.30781655761100957f,
        0.30607682817772597f,
        0.30432828334773276f,
        0.30257118714433284f,
        0.30080580364187f,
        0.2990323969009004f,
        0.2972512309037446f,
        0.29546256949044136f,
        0.2936666762951241f,
        0.29186381468284217f,
        0.29005424768684485f,
        0.28823823794635123f,
        0.28641604764482365f,
        0.28458793844876473f,
        0.28275417144705794f,
        0.28091500709086953f,
        0.2790707051341302f,
        0.2772215245746158f,
        0.2753677235956436f,
        0.2735095595084014f,
        0.2716472886949274f,
        0.26978116655175655f,
        0.2679114474342497f,
        0.26603838460162144f,
        0.2641622301626818f,
        0.2622832350223072f,
        0.26040164882865413f,
        0.2585177199211308f,
        0.2566316952791402f,
        0.2547438204716065f,
        0.2528543396073f,
        0.2509634952859709f,
        0.24907152855030454f,
        0.24717867883871014f,
        0.2452851839389532f,
        0.24339127994264306f,
        0.24149720120058496f,
        0.23960318027900687f,
        0.23770944791667026f,
        0.23581623298287352f,
        0.23392376243635668f,
        0.23203226128511475f,
        0.23014195254712805f,
        0.22825305721201555f,
        0.22636579420361877f,
        0.22448038034352158f,
        0.22259703031551226f,
        0.22071595663099206f,
        0.2188373695953363f,
        0.2169614772752113f,
        0.21508848546685153f,
        0.21321859766530027f,
        0.2113520150346169f,
        0.20948893637905314f,
        0.2076295581152005f,
        0.2057740742451112f,
        0.20392267633039232f,
        0.20207555346727554f,
        0.2002328922626626f,
        0.1983948768111455f,
        0.1965616886730021f,
        0.19473350685316576f,
        0.192910507781168f,
        0.19109286529205227f,
        0.18928075060825697f,
        0.18747433232246544f,
        0.18567377638141933f,
        0.18387924607069356f,
        0.18209090200042746f,
        0.18030890209200973f,
        0.17853340156571182f,
        0.17676455292926568f,
        0.1750025059673803f,
        0.17324740773219213f,
        0.17149940253464305f,
        0.1697586319367805f,
        0.16802523474497263f,
        0.16629934700403215f,
        0.16458110199224193f,
        0.1628706302172743f,
        0.1611680594129971f,
        0.15947351453715822f,
        0.15778711776993998f,
        0.15610898851337562f,
        0.154439243391618f,
        0.15277799625205232f,
        0.15112535816724304f,
        0.14948143743770526f,
        0.14784633959549104f,
        0.14622016740858024f,
        0.14460302088606564f,
        0.14299499728412188f,
        0.1413961911127468f,
        0.13980669414326538f,
        0.1382265954165837f,
        0.13665598125218248f,
        0.13509493525783814f,
        0.13354353834005975f,
        0.13200186871522993f,
        0.13047000192143737f,
        0.12894801083098936f,
        0.12743596566359075f,
        0.12593393400017808f,
        0.12444198079739531f,
        0.12296016840269851f,
        0.12148855657007684f,
        0.12002720247637659f,
        0.11857616073821506f,
        0.1171354834294712f,
        0.1157052200993395f,
        0.114285417790934f,
        0.11287612106042864f,
        0.11147737199672052f,
        0.11008921024160268f,
        0.10871167301043234f,
        0.10734479511328113f,
        0.1059886089765538f,
        0.10464314466506122f,
        0.10330842990453411f,
        0.10198449010456379f,
        0.10067134838195604f,
        0.09936902558448425f,
        0.0980775403150281f,
        0.09679690895608406f,
        0.09552714569463379f,
        0.09426826254735693f,
        0.09302026938617428f,
        0.09178317396410805f,
        0.09055698194144524f,
        0.08934169691219071f,
        0.0881373204307967f,
        0.08694385203915467f,
        0.08576128929383685f,
        0.08458962779357368f,
        0.08342886120695409f,
        0.0822789813003355f,
        0.08113997796595035f,
        0.08001183925019638f,
        0.0788945513820977f,
        0.07778809880192387f,
        0.0766924641899544f,
        0.07560762849537611f,
        0.07453357096530094f,
        0.07347026917389209f,
        0.0724176990515858f,
        0.07137583491439758f,
        0.07034464949330013f,
        0.06932411396366181f,
        0.06831419797473372f,
        0.0673148696791741f,
        0.0663260957625987f,
        0.06534784147314576f,
        0.06438007065104494f,
        0.0634227457581791f,
        0.06247582790762827f,
        0.06153927689318547f,
        0.060613051218833744f,
        0.05969710812817459f,
        0.0587914036337974f,
        0.05789589254658028f,
        0.05701052850491276f,
        0.05613526400383048f,
        0.05527005042405302f,
        0.054414838060915446f,
        0.053569576153184834f,
        0.05273421291175291f,
        0.05190869554819628f,
        0.05109297030319617f,
        0.05028698247480892f,
        0.04949067644657963f,
        0.04870399571549143f,
        0.047926882919742154f,
        0.047159279866341525f,
        0.04640112755852123f,
        0.0456523662229513f,
        0.044912935336755916f,
        0.044182773654321404f,
        0.04346181923389076f,
        0.04275000946393813f,
        0.042047281089317434f,
        0.041353570237178644f,
        0.04066881244264677f,
        0.03999294267425798f,
        0.03932589535914722f,
        0.03866760440798265f,
        0.03801800323964162f,
        0.03737702480562407f,
        0.036744601614198284f,
        0.0361206657542751f,
        0.03550514891900595f,
        0.03489798242910156f,
        0.034299097255866884f,
        0.033708424043948984f,
        0.033125893133794375f,
        0.03255143458381279f,
        0.031984978192244364f,
        0.03142645351872707f,
        0.03087578990556199f,
        0.030332916498673985f,
        0.029797762268265478f,
        0.029270256029160772f,
        0.028750326460839443f,
        0.028237902127156742f,
        0.02773291149574952f,
        0.02723528295712593f,
        0.026744944843437687f,
        0.02626182544693405f,
        0.025785853038096006f,
        0.02531695588345019f,
        0.024855062263061407f,
        0.024400100487703875f,
        0.023951998915710208f,
        0.023510685969497996f,
        0.02307609015177397f,
        0.02264814006141581f,
        0.02222676440903168f,
        0.02181189203219763f,
        0.02140345191037351f,
        0.021001373179497945f,
        0.020605585146262845f,
        0.0202160173020685f,
        0.019832599336659852f,
        0.019455261151445417f,
        0.019083932872499668f,
        0.018718544863250305f,
        0.018359027736851587f,
        0.018005312368245565f,
        0.017657329905912563f,
        0.01731501178331255f,
        0.016978289730019254f,
        0.016647095782549085f,
        0.016321362294886597f,
        0.016001021948708605f,
        0.015686007763309147f,
        0.01537625310522766f,
        0.015071691697582445f,
        0.01477225762911202f,
        0.014477885362926692f,
        0.014188509744973016f,
        0.013904066012213808f,
        0.013624489800526189f,
        0.01334971715232059f,
        0.013079684523883521f,
        0.012814328792447023f,
        0.012553587262987598f,
        0.012297397674757666f,
        0.012045698207552783f,
        0.011798427487717466f,
        0.0115555245938929f,
        0.011316929062509659f,
        0.011082580893028776f,
        0.010852420552934354f,
        0.010626388982481028f,
        0.010404427599199633f,
        0.010186478302164537f,
        0.009972483476025959f,
        0.009762385994810748f,
        0.009556129225495048f,
        0.009353657031352429f,
        0.009154913775080991f,
        0.008959844321712818f,
        0.008768394041309508f,
        0.0085805088114473f,
        0.008396135019495364f,
        0.008215219564690799f,
        0.008037709860013986f,
        0.007863553833867946f,
        0.007692699931565255f,
        0.007525097116626139f,
        0.0073606948718913405f,
        0.0071994432004534835f,
        0.007041292626410458f,
        0.00688619419544446f,
        0.00673409947523025f,
        0.0065849605556763625f,
        0.006438730049002751f,
        0.0062953610896584675f,
        0.006154807334082934f,
        0.006017022960314446f,
        0.005881962667449388f,
        0.005749581674955691f,
        0.005619835721844013f,
        0.005492681065700254f,
        0.005368074481582783f,
        0.005245973260787816f,
        0.005126335209486422f,
        0.005009118647236573f,
        0.004894282405373627f,
        0.004781785825282496f,
        0.004671588756554979f,
        0.00456365155503548f,
        0.004457935080758396f,
        0.0043544006957804355f,
        0.004253010261911042f,
        0.004153726138344199f,
        0.004056511179194654f,
        0.003961328730941773f,
        0.0038681426297840424f,
        0.0037769171989073106f,
        0.003687617245669815f,
        0.0036002080587068832f,
        0.0035146554049583175f,
        0.0034309255266213863f,
        0.003348985138032263f,
        0.003268801422478729f,
        0.0031903420289469513f,
        0.00311357506880513f,
        0.0030384691124266793f,
        0.0029649931857556313f,
        0.002893116766816901f,
        0.002822809782174038f,
        0.002754042603336996f,
        0.0026867860431224197f,
        0.002621011351968954f,
        0.002556690214209995f,
        0.002493794744306305f,
        0.00243229748304078f,
        0.0023721713936777347f,
        0.002313389858088947f,
        0.0022559266728487235f,
        0.0021997560453001f,
        0.0021448525895943773f,
        0.0020911913227060963f,
        0.0020387476604254717f,
        0.001987497413330323f,
        0.001937416782739479f,
        0.0018884823566495828f,
        0.0018406711056572096f,
        0.0017939603788681162f,
        0.001748327899795431f,
        0.0017037517622486098f,
        0.0016602104262148129f,
        0.0016176827137344306f,
        0.0015761478047723872f,
        0.0015355852330868464f,
        0.0014959748820968712f,
        0.001457296980750563f,
        0.0014195320993951697f,
        0.0013826611456506177f,
        0.0013466653602878732f,
        0.0013115263131134962f,
        0.0012772258988617128f,
        0.001243746333095337f,
        0.0012110701481167542f,
        0.0011791801888902161f,
        0.0011480596089766058f,
        0.001117691866481844f,
        0.0010880607200200465f,
        0.001059150224692475f,
        0.0010309447280833599f,
        0.001003428866273584f,
        0.0009765875598732037f,
        0.0009504060100737303f,
        0.0009248696947210947f,
        0.00089996436441016f,
        0.0008756760386016163f,
        0.0008519910017620717f,
        0.0008288957995281015f,
        0.0008063772348950203f,
        0.0007844223644310747f,
        0.0007630184945177477f,
        0.0007421531776168175f,
        0.0007218142085648148f,
        0.0007019896208954637f,
        0.0006826676831906661f,
        0.0006638368954605862f,
        0.0006454859855533397f,
        0.0006276039055947795f,
        0.000610179828458829f,
        0.0005932031442688042f,
        0.0005766634569301406f,
        0.0005605505806949011f,
        0.0005448545367584255f,
        0.0005295655498884626f,
        0.000514674045087103f,
        0.0005001706442857968f,
        0.0004860461630737295f,
        0.00047229160745979894f,
        0.0004588981706684316f,
        0.000445857229969429f,
        0.00043316034354204326f,
        0.00042079924737343605f,
        0.00040876585219167655f,
        0.0003970522404334046f,
        0.00038565066324626403f,
        0.00037455353752620027f,
        0.00036375344298970067f,
        0.00035324311928103177f,
        0.00034301546311451127f,
        0.00033306352545184423f,
        0.0003233805087145333f,
        0.00031395976403135624f,
        0.0003047947885208891f,
        0.00029587922260903844f,
        0.00028720684738154403f,
        0.0002787715819713808f,
        0.0002705674809809877f,
        0.00026258873193923856f,
        0.00025482965279305326f,
        0.00024728468943354277f,
        0.00023994841325655853f,
        0.00023281551875751883f,
        0.00022588082116036564f,
        0.0002191392540805011f,
        0.00021258586722153612f,
        0.00020621582410567757f,
        0.00020002439983757774f,
        0.00019400697890145104f,
        0.00018815905299125616f,
        0.00018247621887374133f,
        0.00017695417628413716f,
        0.0001715887258542725f,
        0.00016637576707288518f,
        0.00016131129627789266f,
        0.00015639140468037913f,
        0.00015161227642005386f,
        0.00014697018665192095f,
        0.00014246149966390712f,
        0.00013808266702517921f,
        0.00013383022576488537f
    },
    0.0f,
    4.0f
};