/* Create extension */
CREATE EXTENSION pg_ac;
/* Init storage */
SELECT ac_init();
/* Create test table with 1000 words for PostgreSQL Full Text Search */
CREATE TABLE test_table5
(
    id SERIAL PRIMARY KEY,
    tsv TSVECTOR
);
INSERT INTO test_table5 (tsv)
SELECT 
to_tsvector('english', 
'all and boy car cat day end family home it man name one people read school speak the this you
ask book can dog eye first go he child in learn morning open play question room say start today word
about at brother drink easy father girl help chair know my new paper please rich she show son they what
always be body careful cry door everything face her if many no pen place road stop student two want where
answer between clear country dance do each friend his job life more park person ready second soon that we why
able before but clean close dream eight for hand inside now or picture river ship shop sit table very write
air black cinema daughter eat from good head cheese important land money pay problem run same see send thing work
any as better cold come doctor find game idea kind live (v) make peace popular right small so some there wait
again back could document egg fire give chance information light may often prefer put red stone such think understand visit
around best call cut dinner down explain get interesting long move out page reach rest set should stand time up
age because big camera city dress evening free have ill like mother old police remember street study teacher voice water
also box class difficult drive food great happy change juice meet need pretty quite real sad spring star take yes
action alone breakfast continue dead enjoy full garden house journey much nothing phone price result sister sun tell view with
against bus company desert expensive flower green church impossible leave month on plan possible return save sea something together woman
anything army bad cover culture decision example feel how island member next position present (adj) record (n) sleep sweet try under world
after bed buy catch corner distance education fast here interest letter never part president round several sound story talk week
almost bread control dear every few gold chief invite late most only product public receive sorry strong then too way
across art bring carry confirm die east group hope industry look must own personal reason service shall stay their wife
away beautiful care cost deep enough fight garage into keep miss other player rather remain side south true use who
already become cause certain describe dry expect fact hard include let moment power provide report seat single system through which
apple blue clock colour different earth film glad hour just love number pencil quick rain simple summer town tree window
address building computer cross desk ear fish glass ice key minute (n) office parent post rock search sport tea valley walk
airport baby card central direction dollar fruit gift high illness milk not piece protect race since slow smile ticket well
accident blood business during even floor general choose inform little meeting order party pink reply snow sugar travel virus watch
another believe both crazy cup decide ever field heart imagine line meat over pull ring sell similar speed than your
above begin century consider dangerous dark exchange government hear jump material near past produce remove secret song television value when
arm behind case collect draw examine fall grow immediately low mind off pass radio shoe station sure test usual while
ago along bear condition direct edge fine half chicken increase magazine nature plate poor respect sharp sometimes still tall would
add among built common depend early fly happen check introduce less mark patient perhaps rise sense short state turn will
act appear break course court discuss effect form hold insect mean once purpose really ride situation success though upon war
afternoon busy coffee detail especially finish ground holiday choice kitchen lesson music orange perfect (adj) request season sick tomorrow welcome yesterday
agree bridge cake customer date enter future gentleman hair image language market plane private restaurant size sky smart thank weather
actor bottle cloth coat destroy everywhere finger guide improve knife large mistake ocean plant repeat salt special teach uncle winter
angry article build dirty except famous gas hotel cheap interview moon nice prepare prison rice seem skirt strange train warm
account bird cloud comfortable damage dust exercise favourite hospital joke message night paint pleasure relationship science serious spend tired wrong
amount bank brown crowd deal engine follow chocolate individual left meal oil pain probably replace society square step temperature university
accept advance bag captain centre demand enemy factory hungry illegal law nose petrol proud responsible store successful swim top win
available boat borrow coast cream design expression farm history injure map obtain peaceful practise recently shape silver smoke touch wash
advantage attack butter club college degree escape gate independent listen marry object path quiet refuse subject (n) supply taste usually vegetable
arrange below cigarette cottage department earn front gentle hat instrument machine newspaper parcel religion repair serve shoulder trip village wall
arrive born clothes correct double English forget goal hate kill last main pair promise regular somewhere space these useful without
animal beer calm copy dish express foreign guess husband lie mine opinion passenger press rule sign support those wonderful year
afraid board circle count death discover funny guest horse lake modern necessary plenty profit reduce share steal trust wish young
admire allow battle climb complete divide effort fresh hole indeed marriage outside pleasant point recent secretary sing soft third various
adventure although bottom coin comfort drop equal gun intelligent join laugh middle perform plain row soldier surface thick until wild
attempt bill breathe cook defend fat grey hot character import lose mountain operation prize risk safe suddenly suit type wood
area asleep bath careless delay event foot hide chain international match nervous pity prove raise shut smell straight trade variety
admit attend branch coal consist declare exact farmer instead jacket leg metal opposite pound roll score shoot speech toilet whose
attitude brave contain doubt experience flat guard heavy charge iron medicine noise politics pour rush smooth spread suggest trouble west
average belong certainly crime duty either fail health influence leader measure offer pile regard rough series spoil spot thin vote')
FROM generate_series(1, 100);
/* Create test table with 1000 words for pg_ac */
CREATE TABLE test_table6
(
    id SERIAL PRIMARY KEY,
    hid INTEGER
);
INSERT INTO test_table6 (hid)
SELECT
ac_build(to_tsvector('english', 
'all and boy car cat day end family home it man name one people read school speak the this you
ask book can dog eye first go he child in learn morning open play question room say start today word
about at brother drink easy father girl help chair know my new paper please rich she show son they what
always be body careful cry door everything face her if many no pen place road stop student two want where
answer between clear country dance do each friend his job life more park person ready second soon that we why
able before but clean close dream eight for hand inside now or picture river ship shop sit table very write
air black cinema daughter eat from good head cheese important land money pay problem run same see send thing work
any as better cold come doctor find game idea kind live (v) make peace popular right small so some there wait
again back could document egg fire give chance information light may often prefer put red stone such think understand visit
around best call cut dinner down explain get interesting long move out page reach rest set should stand time up
age because big camera city dress evening free have ill like mother old police remember street study teacher voice water
also box class difficult drive food great happy change juice meet need pretty quite real sad spring star take yes
action alone breakfast continue dead enjoy full garden house journey much nothing phone price result sister sun tell view with
against bus company desert expensive flower green church impossible leave month on plan possible return save sea something together woman
anything army bad cover culture decision example feel how island member next position present (adj) record (n) sleep sweet try under world
after bed buy catch corner distance education fast here interest letter never part president round several sound story talk week
almost bread control dear every few gold chief invite late most only product public receive sorry strong then too way
across art bring carry confirm die east group hope industry look must own personal reason service shall stay their wife
away beautiful care cost deep enough fight garage into keep miss other player rather remain side south true use who
already become cause certain describe dry expect fact hard include let moment power provide report seat single system through which
apple blue clock colour different earth film glad hour just love number pencil quick rain simple summer town tree window
address building computer cross desk ear fish glass ice key minute (n) office parent post rock search sport tea valley walk
airport baby card central direction dollar fruit gift high illness milk not piece protect race since slow smile ticket well
accident blood business during even floor general choose inform little meeting order party pink reply snow sugar travel virus watch
another believe both crazy cup decide ever field heart imagine line meat over pull ring sell similar speed than your
above begin century consider dangerous dark exchange government hear jump material near past produce remove secret song television value when
arm behind case collect draw examine fall grow immediately low mind off pass radio shoe station sure test usual while
ago along bear condition direct edge fine half chicken increase magazine nature plate poor respect sharp sometimes still tall would
add among built common depend early fly happen check introduce less mark patient perhaps rise sense short state turn will
act appear break course court discuss effect form hold insect mean once purpose really ride situation success though upon war
afternoon busy coffee detail especially finish ground holiday choice kitchen lesson music orange perfect (adj) request season sick tomorrow welcome yesterday
agree bridge cake customer date enter future gentleman hair image language market plane private restaurant size sky smart thank weather
actor bottle cloth coat destroy everywhere finger guide improve knife large mistake ocean plant repeat salt special teach uncle winter
angry article build dirty except famous gas hotel cheap interview moon nice prepare prison rice seem skirt strange train warm
account bird cloud comfortable damage dust exercise favourite hospital joke message night paint pleasure relationship science serious spend tired wrong
amount bank brown crowd deal engine follow chocolate individual left meal oil pain probably replace society square step temperature university
accept advance bag captain centre demand enemy factory hungry illegal law nose petrol proud responsible store successful swim top win
available boat borrow coast cream design expression farm history injure map obtain peaceful practise recently shape silver smoke touch wash
advantage attack butter club college degree escape gate independent listen marry object path quiet refuse subject (n) supply taste usually vegetable
arrange below cigarette cottage department earn front gentle hat instrument machine newspaper parcel religion repair serve shoulder trip village wall
arrive born clothes correct double English forget goal hate kill last main pair promise regular somewhere space these useful without
animal beer calm copy dish express foreign guess husband lie mine opinion passenger press rule sign support those wonderful year
afraid board circle count death discover funny guest horse lake modern necessary plenty profit reduce share steal trust wish young
admire allow battle climb complete divide effort fresh hole indeed marriage outside pleasant point recent secretary sing soft third various
adventure although bottom coin comfort drop equal gun intelligent join laugh middle perform plain row soldier surface thick until wild
attempt bill breathe cook defend fat grey hot character import lose mountain operation prize risk safe suddenly suit type wood
area asleep bath careless delay event foot hide chain international match nervous pity prove raise shut smell straight trade variety
admit attend branch coal consist declare exact farmer instead jacket leg metal opposite pound roll score shoot speech toilet whose
attitude brave contain doubt experience flat guard heavy charge iron medicine noise politics pour rush smooth spread suggest trouble west
average belong certainly crime duty either fail health influence leader measure offer pile regard rough series spoil spot thin vote'))
FROM generate_series(1, 100);
/* Test 1000 words performance */
/* PostgreSQL Full Text Search */
SELECT * FROM test_table5
WHERE tsv @@ 'cat';
EXPLAIN ANALYZE SELECT * FROM test_table5
WHERE tsv @@ 'cat';
/* pg_ac */
SELECT * FROM test_table6
WHERE ac_search(hid, 'cat');
EXPLAIN ANALYZE SELECT * FROM test_table6
WHERE ac_search(hid, 'cat');
SELECT * FROM test_table6
WHERE ac_match(hid, 'cat') IS NOT NULL;
EXPLAIN ANALYZE SELECT * FROM test_table6
WHERE ac_match(hid, 'cat') IS NOT NULL;
SELECT * FROM test_table6
WHERE ac_rank_simple(hid, 'cat') > 0;
EXPLAIN ANALYZE SELECT * FROM test_table6
WHERE ac_rank_simple(hid, 'cat') > 0;
/* Test 1000 words performance */
/* PostgreSQL Full Text Search */
SELECT * FROM test_table5
WHERE tsv @@ to_tsquery('english', 'cat | dog | snake');
EXPLAIN ANALYZE SELECT * FROM test_table5
WHERE tsv @@ to_tsquery('english', 'cat | dog | snake');
/* pg_ac */
SELECT * FROM test_table6
WHERE ac_search(hid, to_tsquery('english', 'cat | dog | snake'));
EXPLAIN ANALYZE SELECT * FROM test_table6
WHERE ac_search(hid, to_tsquery('english', 'cat | dog | snake'));
SELECT * FROM test_table6
WHERE ac_search(hid, 'cat dog snake');
EXPLAIN ANALYZE SELECT * FROM test_table6
WHERE ac_search(hid, 'cat dog snake');
SELECT * FROM test_table6
WHERE ac_match(hid, 'cat dog snake') IS NOT NULL;
EXPLAIN ANALYZE SELECT * FROM test_table6
WHERE ac_match(hid, 'cat dog snake') IS NOT NULL;
SELECT * FROM test_table6
WHERE ac_rank_simple(hid, 'cat dog snake') > 0;
EXPLAIN ANALYZE SELECT * FROM test_table6
WHERE ac_rank_simple(hid, 'cat dog snake') > 0;
/* Clean up */
SELECT ac_fini();
DROP TABLE test_table5 CASCADE;
DROP TABLE test_table6 CASCADE;
DROP EXTENSION pg_ac;