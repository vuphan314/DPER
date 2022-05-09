#include "dmc.hh"

/* global vars ============================================================== */

Int dotFileIndex = 1;

bool existRandom;
bool maximizingAssignment;
Int threadCount;
Int threadSliceCount;
string ddPackage;
Float memSensitivity;
Float maxMem;
string joinPriority;
Int verboseJoinTree;
Int verboseProfiling;

/* classes for processing join trees ======================================== */

/* class JoinTree =========================================================== */

JoinNode* JoinTree::getJoinNode(Int nodeIndex) const {
  if (joinTerminals.contains(nodeIndex)) {
    return joinTerminals.at(nodeIndex);
  }
  return joinNonterminals.at(nodeIndex);
}

JoinNonterminal* JoinTree::getJoinRoot() const {
  return joinNonterminals.at(declaredNodeCount - 1);
}

void JoinTree::printTree() const {
  cout << "c p " << JT_WORD << " " << declaredVarCount << " " << declaredClauseCount << " " << declaredNodeCount << "\n";
  getJoinRoot()->printSubtree("c ");
}

JoinTree::JoinTree(Int declaredVarCount, Int declaredClauseCount, Int declaredNodeCount) {
  this->declaredVarCount = declaredVarCount;
  this->declaredClauseCount = declaredClauseCount;
  this->declaredNodeCount = declaredNodeCount;
}

/* class JoinTreeProcessor ================================================== */

Int JoinTreeProcessor::plannerPid = MIN_INT;
JoinTree* JoinTreeProcessor::joinTree = nullptr;
JoinTree* JoinTreeProcessor::backupJoinTree = nullptr;

void JoinTreeProcessor::killPlanner() {
  if (plannerPid == MIN_INT) {
    cout << WARNING << "found no pid for planner process\n";
  }
  else if (kill(plannerPid, SIGKILL) == 0) {
    cout << "c killed planner process with pid " << plannerPid << "\n";
  }
  else {
    // cout << WARNING << "failed to kill planner process with pid " << plannerPid << "\n";
  }
}

void JoinTreeProcessor::handleSigalrm(int signal) {
  assert(signal == SIGALRM);
  cout << "c received SIGALRM after " << util::getDuration(toolStartPoint) << "s\n";

  if (joinTree == nullptr && backupJoinTree == nullptr) {
    cout << "c found no join tree yet; will wait for first join tree then kill planner\n";
  }
  else {
    cout << "c found join tree; killing planner\n";
    killPlanner();
  }
}

bool JoinTreeProcessor::hasDisarmedTimer() {
  struct itimerval curr_value;
  getitimer(ITIMER_REAL, &curr_value);

  return curr_value.it_value.tv_sec == 0 && curr_value.it_value.tv_usec == 0 && curr_value.it_interval.tv_sec == 0 && curr_value.it_interval.tv_usec == 0;
}

void JoinTreeProcessor::setTimer(Float seconds) {
  assert(seconds >= 0);

  Int secs = seconds;
  Int usecs = (seconds - secs) * 1000000;

  struct itimerval new_value;
  new_value.it_value.tv_sec = secs;
  new_value.it_value.tv_usec = usecs;
  new_value.it_interval.tv_sec = 0;
  new_value.it_interval.tv_usec = 0;

  setitimer(ITIMER_REAL, &new_value, nullptr);
}

void JoinTreeProcessor::armTimer(Float seconds) {
  assert(seconds > 0);
  signal(SIGALRM, handleSigalrm);
  setTimer(seconds);
}

void JoinTreeProcessor::disarmTimer() {
  setTimer(0);
  cout << "c disarmed timer\n";
}

const JoinNonterminal* JoinTreeProcessor::getJoinTreeRoot() const {
  return joinTree->getJoinRoot();
}

void JoinTreeProcessor::processCommentLine(const vector<string>& words) {
  if (words.size() == 3) {
    string key = words.at(1);
    string val = words.at(2);
    if (key == "pid") {
      plannerPid = stoll(val);
    }
    else if (key == "joinTreeWidth") {
      if (joinTree != nullptr) {
        joinTree->width = stoll(val);
      }
    }
    else if (key == "seconds") {
      if (joinTree != nullptr) {
        joinTree->plannerDuration = stold(val);
      }
    }
  }
}

void JoinTreeProcessor::processProblemLine(const vector<string>& words) {
  if (problemLineIndex != MIN_INT) {
    throw MyError("multiple problem lines: ", problemLineIndex, " and ", lineIndex);
  }
  problemLineIndex = lineIndex;

  if (words.size() != 5) {
    throw MyError("problem line ", lineIndex, " has ", words.size(), " words (should be 5)");
  }

  string jtWord = words.at(1);
  if (jtWord != JT_WORD) {
    throw MyError("expected '", JT_WORD, "'; found '", jtWord, "' | line ", lineIndex);
  }

  Int declaredVarCount = stoll(words.at(2));
  Int declaredClauseCount = stoll(words.at(3));
  Int declaredNodeCount = stoll(words.at(4));

  joinTree = new JoinTree(declaredVarCount, declaredClauseCount, declaredNodeCount);

  for (Int terminalIndex = 0; terminalIndex < declaredClauseCount; terminalIndex++) {
    joinTree->joinTerminals[terminalIndex] = new JoinTerminal();
  }
}

void JoinTreeProcessor::processNonterminalLine(const vector<string>& words) {
  if (problemLineIndex == MIN_INT) {
    string message = "no problem line before internal node | line " + to_string(lineIndex);
    if (joinTreeEndLineIndex != MIN_INT) {
      message += " (previous join tree ends on line " + to_string(joinTreeEndLineIndex) + ")";
    }
    throw MyError(message);
  }

  Int parentIndex = stoll(words.front()) - 1; // 0-indexing
  if (parentIndex < joinTree->declaredClauseCount || parentIndex >= joinTree->declaredNodeCount) {
    throw MyError("wrong internal-node index | line ", lineIndex);
  }

  vector<JoinNode*> children;
  Set<Int> projectionVars;
  bool parsingElimVars = false;
  for (Int i = 1; i < words.size(); i++) {
    string word = words.at(i);
    if (word == VAR_ELIM_WORD) {
      parsingElimVars = true;
    }
    else {
      Int num = stoll(word);
      if (parsingElimVars) {
        Int declaredVarCount = joinTree->declaredVarCount;
        if (num <= 0 || num > declaredVarCount) {
          throw MyError("var '", num, "' inconsistent with declared var count '", declaredVarCount, "' | line ", lineIndex);
        }
        projectionVars.insert(num);
      }
      else {
        Int childIndex = num - 1; // 0-indexing
        if (childIndex < 0 || childIndex >= parentIndex) {
          throw MyError("child '", word, "' wrong | line ", lineIndex);
        }
        children.push_back(joinTree->getJoinNode(childIndex));
      }
    }
  }
  joinTree->joinNonterminals[parentIndex] = new JoinNonterminal(children, projectionVars, parentIndex);
}

void JoinTreeProcessor::finishReadingJoinTree() {
  Int nonterminalCount = joinTree->joinNonterminals.size();
  Int expectedNonterminalCount = joinTree->declaredNodeCount - joinTree->declaredClauseCount;

  if (nonterminalCount < expectedNonterminalCount) {
    cout << WARNING << "missing internal nodes (" << expectedNonterminalCount << " expected, " << nonterminalCount << " found) before current join tree ends on line " << lineIndex << "\n";
  }
  else {
    if (joinTree->width == MIN_INT) {
      joinTree->width = joinTree->getJoinRoot()->getWidth();
    }

    cout << "c processed join tree ending on line " << lineIndex << "\n";
    util::printRow("joinTreeWidth", joinTree->width);
    util::printRow("plannerSeconds", joinTree->plannerDuration);

    if (verboseJoinTree >= PARSED_INPUT) {
      cout << THIN_LINE;
      joinTree->printTree();
      cout << THIN_LINE;
    }

    joinTreeEndLineIndex = lineIndex;
    backupJoinTree = joinTree;
    JoinNode::resetStaticFields();
  }

  problemLineIndex = MIN_INT;
  joinTree = nullptr;
}

void JoinTreeProcessor::readInputStream() {
  string line;
  while (getline(std::cin, line)) {
    lineIndex++;

    if (verboseJoinTree >= RAW_INPUT) {
      util::printInputLine(line, lineIndex);
    }

    vector<string> words = util::splitInputLine(line);
    if (words.empty()) {}
    else if (words.front() == "=") { // LG's tree separator "="
      if (joinTree != nullptr) {
        finishReadingJoinTree();
      }
      if (hasDisarmedTimer()) { // timer expires before first join tree ends
        break;
      }
    }
    else if (words.front() == "c") { // possibly special comment line
      processCommentLine(words);
    }
    else if (words.front() == "p") { // problem line
      processProblemLine(words);
    }
    else { // nonterminal-node line
      processNonterminalLine(words);
    }
  }

  if (joinTree != nullptr) {
    finishReadingJoinTree();
  }

  if (!hasDisarmedTimer()) { // stdin ends before timer expires
    cout << "c stdin ends before timer expires; disarming timer\n";
    disarmTimer();
  }
}

JoinTreeProcessor::JoinTreeProcessor(Float plannerWaitDuration) {
  cout << "c procressing join tree...\n";

  armTimer(plannerWaitDuration);
  cout << "c getting join tree from stdin with " << plannerWaitDuration << "s timer (end input with 'enter' then 'ctrl d')\n";

  readInputStream();

  if (joinTree == nullptr) {
    if (backupJoinTree == nullptr) {
      throw MyError("no join tree before line ", lineIndex);
    }
    joinTree = backupJoinTree;
    JoinNode::restoreStaticFields();
  }

  cout << "c getting join tree from stdin: done\n";
}

/* classes for decision diagrams ============================================ */

/* class Dd ================================================================= */

Dd::Dd(const ADD& cuadd) {
  assert(ddPackage == CUDD);
  this->cuadd = cuadd;
}

Dd::Dd(const Mtbdd& mtbdd) {
  assert(ddPackage == SYLVAN);
  this->mtbdd = mtbdd;
}

Dd::Dd(const Dd& dd) {
  if (ddPackage == CUDD) {
    this->cuadd = dd.cuadd;
  }
  else {
    this->mtbdd = dd.mtbdd;
  }
}

const Cudd* Dd::newMgr(Float mem, Int threadIndex) {
  assert(ddPackage == CUDD);
  Cudd* mgr = new Cudd(
    0, // init num of BDD vars
    0, // init num of ZDD vars
    CUDD_UNIQUE_SLOTS, // init num of unique-table slots; cudd.h: #define CUDD_UNIQUE_SLOTS 256
    CUDD_CACHE_SLOTS, // init num of cache-table slots; cudd.h: #define CUDD_CACHE_SLOTS 262144
    mem * MEGA // maxMemory
  );
  mgr->getManager()->threadIndex = threadIndex;
  mgr->getManager()->peakMemIncSensitivity = memSensitivity * MEGA; // makes CUDD print "c cuddMegabytes_{threadIndex + 1} {memused / 1e6}"
  if (verboseSolving >= 1 && threadIndex == 0) {
    // util::printRow("hardMaxMemMegabytes", mgr->ReadMaxMemory() / MEGA); // for unique table and cache table combined (unlimited by default)
    // util::printRow("softMaxMemMegabytes", mgr->getManager()->maxmem / MEGA); // cuddInt.c: maxmem = maxMemory / 10 * 9
    // util::printRow("hardMaxCacheMegabytes", mgr->ReadMaxCacheHard() * sizeof(DdCache) / MEGA); // cuddInt.h: #define DD_MAX_CACHE_FRACTION 3
    // writeInfoFile(mgr, "info.txt");
  }
  return mgr;
}

Dd Dd::getConstDd(const Number& n, const Cudd* mgr) {
  if (ddPackage == CUDD) {
    return logCounting ? Dd(mgr->constant(n.getLog10())) : Dd(mgr->constant(n.fraction));
  }
  if (multiplePrecision) {
    mpq_t q; // C interface
    mpq_init(q);
    mpq_set(q, n.quotient.get_mpq_t());
    Dd dd(Mtbdd(mtbdd_gmp(q)));
    mpq_clear(q);
    return dd;
  }
  return Dd(Mtbdd::doubleTerminal(n.fraction));
}

Dd Dd::getZeroDd(const Cudd* mgr) {
  return getConstDd(Number(), mgr);
}

Dd Dd::getOneDd(const Cudd* mgr) {
  return getConstDd(Number("1"), mgr);
}

Dd Dd::getVarDd(Int ddVar, bool val, const Cudd* mgr) {
  if (ddPackage == CUDD) {
    if (logCounting) {
      return Dd(mgr->addLogVar(ddVar, val));
    }
    return val ? Dd(mgr->addVar(ddVar)) : Dd((mgr->addVar(ddVar)).Cmpl());
  }
  if (val) {
    return Dd(mtbdd_makenode(ddVar, getZeroDd(mgr).mtbdd.GetMTBDD(), getOneDd(mgr).mtbdd.GetMTBDD())); // (var, lo, hi)
  }
  return Dd(mtbdd_makenode(ddVar, getOneDd(mgr).mtbdd.GetMTBDD(), getZeroDd(mgr).mtbdd.GetMTBDD()));
}

size_t Dd::countNodes() const {
  if (ddPackage == CUDD) {
    return cuadd.nodeCount();
  }
  return mtbdd.NodeCount();
}

bool Dd::operator<(const Dd& rightDd) const {
  if (joinPriority == SMALLEST_PAIR) { // top = rightmost = smallest
    return countNodes() > rightDd.countNodes();
  }
  return countNodes() < rightDd.countNodes();
}

Number Dd::extractConst() const {
  if (ddPackage == CUDD) {
    ADD minTerminal = cuadd.FindMin();
    assert(minTerminal == cuadd.FindMax());
    return Number(cuddV(minTerminal.getNode()));
  }
  assert(mtbdd.isLeaf());
  if (multiplePrecision) {
    return Number(mpq_class((mpq_ptr)mtbdd_getvalue(mtbdd.GetMTBDD())));
  }
  return Number(mtbdd_getdouble(mtbdd.GetMTBDD()));
}

Dd Dd::getComposition(Int ddVar, bool val, const Cudd* mgr) const {
  if (ddPackage == CUDD) {
    if (util::isFound(ddVar, cuadd.SupportIndices())) {
      return Dd(cuadd.Compose(val ? mgr->addOne() : mgr->addZero(), ddVar));
    }
    return *this;
  }
  sylvan::MtbddMap m;
  m.put(ddVar, val ? Mtbdd::mtbddOne() : Mtbdd::mtbddZero());
  return Dd(mtbdd.Compose(m));
}

Dd Dd::getProduct(const Dd& dd) const {
  if (ddPackage == CUDD) {
    return logCounting ? Dd(cuadd + dd.cuadd) : Dd(cuadd * dd.cuadd);
  }
  if (multiplePrecision) {
    LACE_ME;
    return Dd(Mtbdd(gmp_times(mtbdd.GetMTBDD(), dd.mtbdd.GetMTBDD())));
  }
  return Dd(mtbdd * dd.mtbdd);
}

Dd Dd::getSum(const Dd& dd) const {
  if (ddPackage == CUDD) {
    return logCounting ? Dd(cuadd.LogSumExp(dd.cuadd)) : Dd(cuadd + dd.cuadd);
  }
  if (multiplePrecision) {
    LACE_ME;
    return Dd(Mtbdd(gmp_plus(mtbdd.GetMTBDD(), dd.mtbdd.GetMTBDD())));
  }
  return Dd(mtbdd + dd.mtbdd);
}

Dd Dd::getMax(const Dd& dd) const {
  if (ddPackage == CUDD) {
    return Dd(cuadd.Maximum(dd.cuadd));
  }
  if (multiplePrecision) {
    LACE_ME;
    return Dd(Mtbdd(gmp_max(mtbdd.GetMTBDD(), dd.mtbdd.GetMTBDD())));
  }
  return Dd(mtbdd.Max(dd.mtbdd));
}

Set<Int> Dd::getSupport() const {
  Set<Int> support;
  if (ddPackage == CUDD) {
    for (Int ddVar : cuadd.SupportIndices()) {
      support.insert(ddVar);
    }
  }
  else {
    Mtbdd cube = mtbdd.Support(); // conjunction of all vars appearing in mtbdd
    while (!cube.isOne()) {
      support.insert(cube.TopVar());
      cube = cube.Then();
    }
  }
  return support;
}

Dd Dd::getBoolDiff(const Dd& rightDd) const {
  if (ddPackage == CUDD) {
    Dd gx((cuadd - rightDd.cuadd).BddThreshold(0).Add());
    return gx;
  }
  assert(("unimplemented for Sylvan", false));
}

bool Dd::evalAssignment(vector<int>& ddVarAssignment) const {
  if (ddPackage == CUDD) {
    Number n = Dd(cuadd.Eval(&ddVarAssignment.front())).extractConst();
    return n == Number("1");
  }
  assert(("unimplemented for Sylvan", false));
}

Dd Dd::getAbstraction(Int ddVar, const vector<Int>& ddVarToCnfVarMap, const Map<Int, Number>& literalWeights, const Assignment& assignment, bool additive, vector<pair<Int, Dd>>& maximizerStack, const Cudd* mgr) const {
  Int cnfVar = ddVarToCnfVarMap.at(ddVar);
  Dd positiveWeight = getConstDd(literalWeights.at(cnfVar), mgr);
  Dd negativeWeight = getConstDd(literalWeights.at(-cnfVar), mgr);

  auto it = assignment.find(cnfVar);
  if (it != assignment.end()) {
    Dd weight = it->second ? positiveWeight : negativeWeight;
    return getProduct(weight);
  }

  Dd term0 = getComposition(ddVar, false, mgr).getProduct(negativeWeight);
  Dd term1 = getComposition(ddVar, true, mgr).getProduct(positiveWeight);

  if (maximizingAssignment && !additive) {
    // assert(ddPackage == CUDD);

    Dd gx = term1.getBoolDiff(term0);
    maximizerStack.push_back({ddVar, gx});
  }

  return additive ? term0.getSum(term1) : term0.getMax(term1);
}

void Dd::writeDotFile(const Cudd* mgr, string dotFileDir) const {
  string filePath = dotFileDir + "dd" + to_string(dotFileIndex++) + ".dot";
  FILE* file = fopen(filePath.c_str(), "wb"); // writes to binary file

  if (ddPackage == CUDD) { // davidkebo.com/cudd#cudd6
    DdNode** ddNodeArray = static_cast<DdNode**>(malloc(sizeof(DdNode*)));
    ddNodeArray[0] = cuadd.getNode();
    Cudd_DumpDot(mgr->getManager(), 1, ddNodeArray, NULL, NULL, file);
    free(ddNodeArray);
  }
  else {
    mtbdd_fprintdot_nc(file, mtbdd.GetMTBDD());
  }

  fclose(file);
  cout << "c overwrote file " << filePath << "\n";
}

void Dd::writeInfoFile(const Cudd* mgr, string filePath) {
  assert(ddPackage == CUDD);
  FILE* file = fopen(filePath.c_str(), "w");
  Cudd_PrintInfo(mgr->getManager(), file);
  fclose(file);
  cout << "c overwrote file " << filePath << "\n";
}

/* class Executor =========================================================== */

vector<pair<Int, Dd>> Executor::maximizerStack;

Map<Int, Float> Executor::varDurations;
Map<Int, size_t> Executor::varDdSizes;

void Executor::updateVarDurations(const JoinNode* joinNode, TimePoint startPoint) {
  if (verboseProfiling >= 1) {
    Float duration = util::getDuration(startPoint);
    if (duration > 0) {
      if (verboseProfiling >= 2) {
        util::printRow("joinNodeSeconds_" + to_string(joinNode->nodeIndex + 1), duration);
      }

      for (Int var : joinNode->preProjectionVars) {
        if (varDurations.contains(var)) {
          varDurations[var] += duration;
        }
        else {
          varDurations[var] = duration;
        }
      }
    }
  }
}

void Executor::updateVarDdSizes(const JoinNode* joinNode, const Dd& dd) {
  if (verboseProfiling >= 1) {
    size_t ddSize = dd.countNodes();

    if (verboseProfiling >= 2) {
      util::printRow("joinNodeDiagramSize_" + to_string(joinNode->nodeIndex + 1), ddSize);
    }

    for (Int var : joinNode->preProjectionVars) {
      if (varDdSizes.contains(var)) {
        varDdSizes[var] = max(varDdSizes[var], ddSize);
      }
      else {
        varDdSizes[var] = ddSize;
      }
    }
  }
}

void Executor::printVarDurations() {
  multimap<Float, Int, greater<Float>> timedVars = util::flipMap(varDurations); // duration |-> var
  for (pair<Float, Int> timedVar : timedVars) {
    util::printRow("varTotalSeconds_" + to_string(timedVar.second), timedVar.first);
  }
}

void Executor::printVarDdSizes() {
  multimap<size_t, Int, greater<size_t>> sizedVars = util::flipMap(varDdSizes); // dd size |-> var
  for (pair<size_t, Int> sizedVar : sizedVars) {
    util::printRow("varMaxDiagramSize_" + to_string(sizedVar.second), sizedVar.first);
  }
}

Dd Executor::getClauseDd(const Map<Int, Int>& cnfVarToDdVarMap, const Clause& clause, const Cudd* mgr, const Assignment& assignment) {
  Dd clauseDd = Dd::getZeroDd(mgr);
  for (Int literal : clause) {
    bool val = literal > 0;
    Int cnfVar = abs(literal);
    auto it = assignment.find(cnfVar);
    if (it != assignment.end()) { // slices clause on literal
      if (it->second == val) { // returns satisfied clause
        return Dd::getOneDd(mgr);
      } // excludes unsatisfied literal from clause otherwise
    }
    else {
      Int ddVar = cnfVarToDdVarMap.at(cnfVar);
      Dd literalDd = Dd::getVarDd(ddVar, val, mgr);
      clauseDd = clauseDd.getMax(literalDd);
    }
  }
  return clauseDd;
}

Dd Executor::solveSubtree(const JoinNode* joinNode, const Map<Int, Int>& cnfVarToDdVarMap, const vector<Int>& ddVarToCnfVarMap, const Cudd* mgr, const Assignment& assignment) {
  if (joinNode->isTerminal()) {
    TimePoint terminalStartPoint = util::getTimePoint();

    Dd d = getClauseDd(cnfVarToDdVarMap, JoinNode::cnf.clauses.at(joinNode->nodeIndex), mgr, assignment);

    updateVarDurations(joinNode, terminalStartPoint);
    updateVarDdSizes(joinNode, d);

    return d;
  }

  vector<Dd> childDdList;
  for (JoinNode* child : joinNode->children) {
    childDdList.push_back(solveSubtree(child, cnfVarToDdVarMap, ddVarToCnfVarMap, mgr, assignment));
  }

  TimePoint nonterminalStartPoint = util::getTimePoint();
  Dd dd = Dd::getOneDd(mgr);

  if (joinPriority == ARBITRARY_PAIR) { // arbitrarily multiplies child ADDs
    for (Dd childDd : childDdList) {
      dd = dd.getProduct(childDd);
    }
  }
  else { // Dd::operator< handles both biggest-first and smallest-first
    std::priority_queue<Dd> childDdQueue;
    for (Dd childDd : childDdList) {
      childDdQueue.push(childDd);
    }
    assert(!childDdQueue.empty());
    while (childDdQueue.size() > 1) {
      Dd dd1 = childDdQueue.top();
      childDdQueue.pop();
      Dd dd2 = childDdQueue.top();
      childDdQueue.pop();
      Dd dd3 = dd1.getProduct(dd2);
      childDdQueue.push(dd3);
    }
    dd = childDdQueue.top();
  }

  for (Int cnfVar : joinNode->projectionVars) {
    Int ddVar = cnfVarToDdVarMap.at(cnfVar);

    bool additive = JoinNode::cnf.outerVars.contains(cnfVar);
    if (existRandom) {
      additive = !additive;
    }

    dd = dd.getAbstraction(ddVar, ddVarToCnfVarMap, JoinNode::cnf.literalWeights, assignment, additive, maximizerStack, mgr);
  }

  updateVarDurations(joinNode, nonterminalStartPoint);
  updateVarDdSizes(joinNode, dd);

  return dd;
}

void Executor::solveThreadSlices(const JoinNonterminal* joinRoot, const Map<Int, Int>& cnfVarToDdVarMap, const vector<Int>& ddVarToCnfVarMap, Float threadMem, Int threadIndex, const vector<vector<Assignment>>& threadAssignmentLists, Number& totalSolution, mutex& solutionMutex) {
  const vector<Assignment>& threadAssignments = threadAssignmentLists.at(threadIndex);
  for (Int threadAssignmentIndex = 0; threadAssignmentIndex < threadAssignments.size(); threadAssignmentIndex++) {
    TimePoint sliceStartPoint = util::getTimePoint();

    Number partialSolution = solveSubtree(static_cast<const JoinNode*>(joinRoot), cnfVarToDdVarMap, ddVarToCnfVarMap, Dd::newMgr(threadMem, threadIndex), threadAssignments.at(threadAssignmentIndex)).extractConst();

    const std::lock_guard<mutex> g(solutionMutex);

    if (verboseSolving >= 1) {
      cout << "c thread " << right << setw(4) << threadIndex + 1 << "/" << threadAssignmentLists.size() << " | assignment " << setw(4) << threadAssignmentIndex + 1 << "/" << threadAssignments.size() << ": { ";
      threadAssignments.at(threadAssignmentIndex).printAssignment();
      cout << " }\n";

      cout << "c thread " << right << setw(4) << threadIndex + 1 << "/" << threadAssignmentLists.size() << " | assignment " << setw(4) << threadAssignmentIndex + 1 << "/" << threadAssignments.size() << " | seconds " << left << setw(10) << util::getDuration(sliceStartPoint) << " | mc " << setw(15);
      if (logCounting) {
        cout << exp10l(partialSolution.fraction) << " | log10(mc) " << partialSolution.fraction << "\n";
      }
      else {
        cout << partialSolution << "\n";
      }
    }

    totalSolution = logCounting ? Number(totalSolution.getLogSumExp(partialSolution)) : totalSolution + partialSolution;
  }
}

vector<vector<Assignment>> Executor::getThreadAssignmentLists(const JoinNonterminal* joinRoot, Int sliceVarOrderHeuristic) {
  size_t sliceVarCount = ceill(log2l(threadCount * threadSliceCount));
  sliceVarCount = min(sliceVarCount, JoinNode::cnf.outerVars.size());

  Int remainingSliceCount = exp2l(sliceVarCount);
  Int remainingThreadCount = threadCount;
  vector<Int> threadSliceCounts;
  while (remainingThreadCount > 0) {
    Int sliceCount = ceill(static_cast<Float>(remainingSliceCount) / remainingThreadCount);
    threadSliceCounts.push_back(sliceCount);
    remainingSliceCount -= sliceCount;
    remainingThreadCount--;
  }
  assert(remainingSliceCount == 0);

  if (verboseSolving >= 1) {
    cout << "c thread slice counts: { ";
    for (Int sliceCount : threadSliceCounts) {
      cout << sliceCount << " ";
    }
    cout << "}\n";
  }

  vector<Assignment> assignments = joinRoot->getOuterAssignments(sliceVarOrderHeuristic, sliceVarCount);
  vector<vector<Assignment>> threadAssignmentLists;
  vector<Assignment> threadAssignmentList;
  for (Int assignmentIndex = 0, threadListIndex = 0; assignmentIndex < assignments.size() && threadListIndex < threadSliceCounts.size(); assignmentIndex++) {
    threadAssignmentList.push_back(assignments.at(assignmentIndex));
    if (threadAssignmentList.size() == threadSliceCounts.at(threadListIndex)) {
      threadAssignmentLists.push_back(threadAssignmentList);
      threadAssignmentList.clear();
      threadListIndex++;
    }
  }

  if (verboseSolving >= 2) {
    for (Int threadIndex = 0; threadIndex < threadAssignmentLists.size(); threadIndex++) {
      const vector<Assignment>& threadAssignments = threadAssignmentLists.at(threadIndex);
      cout << "c assignments in thread " << right << setw(4) << threadIndex + 1 << ":";
      for (const Assignment& assignment : threadAssignments) {
        cout << " { ";
        assignment.printAssignment();
        cout << " }";
      }
      cout << "\n";
    }
  }

  return threadAssignmentLists;
}

Number Executor::solveCnf(const JoinNonterminal* joinRoot, const Map<Int, Int>& cnfVarToDdVarMap, const vector<Int>& ddVarToCnfVarMap, Int sliceVarOrderHeuristic) {
  if (ddPackage == SYLVAN) {
    return solveSubtree(
      static_cast<const JoinNode*>(joinRoot),
      cnfVarToDdVarMap,
      ddVarToCnfVarMap
    ).extractConst();
  }

  vector<vector<Assignment>> threadAssignmentLists = getThreadAssignmentLists(joinRoot, sliceVarOrderHeuristic);
  util::printRow("sliceWidth", joinRoot->getWidth(threadAssignmentLists.front().front())); // any assignment would work
  Number totalSolution = logCounting ? Number(-INF) : Number();
  mutex solutionMutex;

  Float threadMem = maxMem / threadAssignmentLists.size();
  util::printRow("threadMaxMemMegabytes", threadMem);

  vector<thread> threads;

  Int threadIndex = 0;
  for (; threadIndex < threadAssignmentLists.size() - 1; threadIndex++) {
    threads.push_back(thread(
      solveThreadSlices,
      std::cref(joinRoot),
      std::cref(cnfVarToDdVarMap),
      std::cref(ddVarToCnfVarMap),
      threadMem,
      threadIndex,
      threadAssignmentLists,
      std::ref(totalSolution),
      std::ref(solutionMutex)
    ));
  }
  solveThreadSlices(
    joinRoot,
    cnfVarToDdVarMap,
    ddVarToCnfVarMap,
    threadMem,
    threadIndex,
    threadAssignmentLists,
    totalSolution,
    solutionMutex
  );
  for (thread& t : threads) {
    t.join();
  }

  return totalSolution;
}

Number Executor::processHiddenVar(const Number &apparentSolution, Int cnfVar, bool additive) {
  if (JoinNode::cnf.apparentVars.contains(cnfVar)) {
    return apparentSolution;
  }

  const Number& positiveWeight = JoinNode::cnf.literalWeights.at(cnfVar);
  const Number& negativeWeight = JoinNode::cnf.literalWeights.at(-cnfVar);
  if (additive) {
    return logCounting ? apparentSolution + (positiveWeight + negativeWeight).getLog10() : apparentSolution * (positiveWeight + negativeWeight);
  }
  else {
    return logCounting ? apparentSolution + max(positiveWeight, negativeWeight).getLog10(): apparentSolution * max(positiveWeight, negativeWeight); // non-negative weights
  }
}

Number Executor::processHiddenVars(const Number &apparentSolution) {
  Number n = apparentSolution;

  for (Int var = 1; var <= JoinNode::cnf.declaredVarCount; var++) { // processes inner vars
    if (!JoinNode::cnf.outerVars.contains(var)) {
      n = processHiddenVar(apparentSolution, var, existRandom);
    }
  }

  for (Int var : JoinNode::cnf.outerVars) {
    if (!JoinNode::cnf.apparentVars.contains(var)) {
      n = processHiddenVar(apparentSolution, var, !existRandom);
    }
  }

  return n;
}

void Executor::printSatRow(const Number& solution, bool surelyUnsat, size_t keyWidth) {
  const string SAT_WORD = "SATISFIABLE";
  const string UNSAT_WORD = "UN" + SAT_WORD;

  string satisfiability = "UNKNOWN";

  if (surelyUnsat) { // empty clause
    satisfiability = UNSAT_WORD;
  }
  else if (logCounting) {
    if (solution.fraction == -INF) {
      if (!weightedCounting) {
        satisfiability = UNSAT_WORD;
      }
    }
    else {
      satisfiability = SAT_WORD;
    }
  }
  else {
    if (solution == Number()) {
      if (!weightedCounting) {
        satisfiability = UNSAT_WORD;
      }
    }
    else {
      satisfiability = SAT_WORD;
    }
  }

  util::printRow("s", satisfiability, keyWidth);
}

void Executor::printTypeRow(size_t keyWidth) {
  util::printRow("s type", projectedCounting ? "pmc" : (weightedCounting ? "wmc" : "mc"), keyWidth);
}

void Executor::printEstRow(const Number& solution, size_t keyWidth) {
  util::printPreciseFloatRow("s log10-estimate", logCounting ? solution.fraction : solution.getLog10(), keyWidth);
}

void Executor::printArbRow(const Number& solution, bool frac, size_t keyWidth) {
  string key = "s exact arb ";

  if (weightedCounting) {
    if (frac) {
      util::printRow(key + "frac", solution, keyWidth);
    }
    else {
      util::printRow(key + "float", mpf_class(solution.quotient), keyWidth);
    }
  }
  else {
    util::printRow(key + "int", solution, keyWidth);
  }
}

void Executor::printDoubleRow(const Number& solution, size_t keyWidth) {
  Float f = solution.fraction;
  util::printPreciseFloatRow("s exact double prec-sci", logCounting ? exp10l(f) : f, keyWidth);
}

void Executor::printSolutionRows(const Number& solution, bool surelyUnsat, size_t keyWidth) {
  cout << THIN_LINE;

  Number n = processHiddenVars(solution);

  printSatRow(n, surelyUnsat, keyWidth);
  printTypeRow(keyWidth);
  printEstRow(n, keyWidth);

  if (multiplePrecision) {
    printArbRow(n, false, keyWidth); // notation = weighted ? int : float
    if (weightedCounting) {
      printArbRow(n, true, keyWidth); // notation = frac
    }
  }
  else {
    printDoubleRow(n, keyWidth);
  }

  cout << THIN_LINE;
}

void Executor::printMaximizerRow(const vector<Int>& ddVarToCnfVarMap) {
  vector<int> ddVarAssignment(ddVarToCnfVarMap.size(), 0);
  vector<Int> poppedDdVars;

  while (maximizerStack.size()) {
    pair<Int, Dd> xAndGx = maximizerStack.back();
    Int x = xAndGx.first; // ddVar
    Dd gx = xAndGx.second;

    if (gx.evalAssignment(ddVarAssignment)) {
      ddVarAssignment[x] = 1;
    }

    poppedDdVars.push_back(x);
    maximizerStack.pop_back();
  }

  cout << "v ";
  for (Int ddVar : poppedDdVars) {
    if (!ddVarAssignment.at(ddVar)) {
      cout << "-";
    }
    cout << ddVarToCnfVarMap.at(ddVar) << " ";
  }
  cout << "0\n";
}

Executor::Executor(const JoinNonterminal* joinRoot, Int ddVarOrderHeuristic, Int sliceVarOrderHeuristic) {
  cout << "\n";
  cout << "c computing output...\n";
  Map<Int, Int> cnfVarToDdVarMap; // e.g. {42: 0, 13: 1}

  TimePoint ddVarOrderStartPoint = util::getTimePoint();
  vector<Int> ddVarToCnfVarMap = joinRoot->getVarOrder(ddVarOrderHeuristic); // e.g. [42, 13], i.e. ddVarOrder
  if (verboseSolving >= 1) {
    util::printRow("diagramVarSeconds", util::getDuration(ddVarOrderStartPoint));
  }

  for (Int ddVar = 0; ddVar < ddVarToCnfVarMap.size(); ddVar++) {
    Int cnfVar = ddVarToCnfVarMap.at(ddVar);
    cnfVarToDdVarMap[cnfVar] = ddVar;
  }

  Number n = solveCnf(joinRoot, cnfVarToDdVarMap, ddVarToCnfVarMap, sliceVarOrderHeuristic);

  printVarDurations();
  printVarDdSizes();

  if (verboseSolving >= 1) {
    util::printRow("apparentSolution", logCounting ? exp10l(n.fraction) : n);
  }

  printSolutionRows(n);

  if (maximizingAssignment) {
    printMaximizerRow(ddVarToCnfVarMap);
  }
}

/* class OptionDict ========================================================= */

string OptionDict::helpDdPackage() {
  string s = "diagram package: ";
  for (auto it = DD_PACKAGES.begin(); it != DD_PACKAGES.end(); it++) {
    s += it->first + "/" + it->second;
    if (next(it) != DD_PACKAGES.end()) {
      s += ", ";
    }
  }
  return s + "; string";
}

string OptionDict::helpJoinPriority() {
  string s = "join priority: ";
  for (auto it = JOIN_PRIORITIES.begin(); it != JOIN_PRIORITIES.end(); it++) {
    s += it->first + "/" + it->second;
    if (next(it) != JOIN_PRIORITIES.end()) {
      s += ", ";
    }
  }
  return s + "; string";
}

void OptionDict::runCommand() const {
  if (verboseSolving >= 1) {
    cout << "c processing command-line options...\n";

    util::printRow("cnfFile", cnfFilePath);
    util::printRow("weightedCounting", weightedCounting);
    util::printRow("projectedCounting", projectedCounting);
    util::printRow("existRandom", existRandom);
    util::printRow("maximizingAssignment", maximizingAssignment);

    util::printRow("plannerWaitSeconds", plannerWaitDuration);

    util::printRow("diagramPackage", DD_PACKAGES.at(ddPackage));

    util::printRow("threadCount", threadCount);

    if (ddPackage == CUDD) {
      util::printRow("threadSliceCount", threadSliceCount);
    }

    util::printRow("randomSeed", randomSeed);

    util::printRow("diagramVarOrder", (ddVarOrderHeuristic < 0 ? "INVERSE_" : "") + CNF_VAR_ORDER_HEURISTICS.at(abs(ddVarOrderHeuristic)));

    if (ddPackage == CUDD) {
      util::printRow("sliceVarOrder", (sliceVarOrderHeuristic < 0 ? "INVERSE_" : "") + util::getVarOrderHeuristics().at(abs(sliceVarOrderHeuristic)));
      util::printRow("memSensitivityMegabytes", memSensitivity);
    }

    util::printRow("maxMemMegabytes", maxMem);

    if (ddPackage == SYLVAN) {
      util::printRow("tableRatio", tableRatio);
      util::printRow("initRatio", initRatio);
      util::printRow("multiplePrecision", multiplePrecision);
    }
    else {
      util::printRow("logCounting", logCounting);
    }

    util::printRow("joinPriority", JOIN_PRIORITIES.at(joinPriority));
    cout << "\n";
  }

  try {
    JoinNode::cnf = Cnf(cnfFilePath);

    if (JoinNode::cnf.clauses.empty()) {
      cout << WARNING << "empty cnf\n";
      Executor::printSolutionRows(logCounting ? Number() : Number("1"));
      return;
    }

    JoinTreeProcessor joinTreeProcessor(plannerWaitDuration);

    if (ddPackage == SYLVAN) { // initializes Sylvan
      lace_init(threadCount, 0);
      lace_startup(0, NULL, NULL);
      sylvan::sylvan_set_limits(maxMem * MEGA, tableRatio, initRatio);
      sylvan::sylvan_init_package();
      sylvan::sylvan_init_mtbdd();
      if (multiplePrecision) {
        sylvan::gmp_init();
      }
    }

    Executor executor(joinTreeProcessor.getJoinTreeRoot(), ddVarOrderHeuristic, sliceVarOrderHeuristic);

    if (ddPackage == SYLVAN) { // quits Sylvan
      sylvan::sylvan_quit();
      lace_exit();
    }
  }
  catch (EmptyClauseException) {
    Executor::printSolutionRows(logCounting ? Number(-INF) : Number(), true);
  }
}

OptionDict::OptionDict(int argc, char** argv) {
  cxxopts::Options options("dmc", "Diagram Model Counter (reads join tree from stdin)");
  options.set_width(105);
  options.add_options()
    (CNF_FILE_OPTION, "cnf file path; string (REQUIRED)", value<string>())
    (WEIGHTED_COUNTING_OPTION, "weighted counting: 0, 1; int", value<Int>()->default_value("0"))
    (PROJECTED_COUNTING_OPTION, "projected counting: 0, 1; int", value<Int>()->default_value("0"))
    (EXIST_RANDOM_OPTION, "exist-random stochastic satisfiability: 0, 1; int", value<Int>()->default_value("0"))
    (MAXIMIZING_ASSIGNMENT_OPTION, "maximizing assignment [with " + EXIST_RANDOM_OPTION + "_arg = 1]: 0, 1; int", value<Int>()->default_value("0"))
    (PLANNER_WAIT_OPTION, "planner wait duration (in seconds); float", value<Float>()->default_value(to_string(MIN_PLANNER_WAIT_DURATION)))
    (DD_PACKAGE_OPTION, helpDdPackage(), value<string>()->default_value(CUDD))
    (THREAD_COUNT_OPTION, "thread count, or 0 for hardware_concurrency value; int", value<Int>()->default_value("1"))
    (THREAD_SLICE_COUNT_OPTION, "thread slice count" + util::useDdPackage(CUDD) + "; int", value<Int>()->default_value("1"))
    (RANDOM_SEED_OPTION, "random seed; int", value<Int>()->default_value("0"))
    (DD_VAR_OPTION, util::helpVarOrderHeuristic("diagram"), value<Int>()->default_value(to_string(MCS)))
    (SLICE_VAR_OPTION, util::helpVarOrderHeuristic("slice"), value<Int>()->default_value(to_string(BIGGEST_NODE)))
    (MEM_SENSITIVITY_OPTION, "mem sensitivity (in MB) for reporting usage" + util::useDdPackage(CUDD) + "; float", value<Float>()->default_value("1e3"))
    (MAX_MEM_OPTION, "max mem (in MB) for unique table and cache table combined; float", value<Float>()->default_value("4e3"))
    (TABLE_RATIO_OPTION, "table ratio" + util::useDdPackage(SYLVAN) + ": log2(unique_size/cache_size); int", value<Int>()->default_value("1"))
    (INIT_RATIO_OPTION, "init ratio for tables" + util::useDdPackage(SYLVAN) + ": log2(max_size/init_size); int", value<Int>()->default_value("10"))
    (MULTIPLE_PRECISION_OPTION, "multiple precision" + util::useDdPackage(SYLVAN) + ": 0, 1; int", value<Int>()->default_value("0"))
    (LOG_COUNTING_OPTION, "log counting" + util::useDdPackage(CUDD) + ": 0, 1; int", value<Int>()->default_value("0"))
    (JOIN_PRIORITY_OPTION, helpJoinPriority(), value<string>()->default_value(SMALLEST_PAIR))
    (VERBOSE_CNF_OPTION, "verbose cnf processing: 0, " + INPUT_VERBOSITIES, value<Int>()->default_value("0"))
    (VERBOSE_JOIN_TREE_OPTION, "verbose join-tree processing: 0, " + INPUT_VERBOSITIES, value<Int>()->default_value("0"))
    (VERBOSE_PROFILING_OPTION, "verbose profiling: 0, 1, 2; int", value<Int>()->default_value("0"))
    (VERBOSE_SOLVING_OPTION, util::helpVerboseSolving(), value<Int>()->default_value("1"))
  ;
  cxxopts::ParseResult result = options.parse(argc, argv);
  if (result.count(CNF_FILE_OPTION)) {
    cnfFilePath = result[CNF_FILE_OPTION].as<string>();

    weightedCounting = result[WEIGHTED_COUNTING_OPTION].as<Int>(); // global var
    projectedCounting = result[PROJECTED_COUNTING_OPTION].as<Int>(); // global var

    existRandom = result[EXIST_RANDOM_OPTION].as<Int>(); // global var
    maximizingAssignment = result[MAXIMIZING_ASSIGNMENT_OPTION].as<Int>(); // global var
    assert(!maximizingAssignment || existRandom);

    plannerWaitDuration = result[PLANNER_WAIT_OPTION].as<Float>();
    if (plannerWaitDuration <= 0) {
      plannerWaitDuration = MIN_PLANNER_WAIT_DURATION;
    }

    ddPackage = result[DD_PACKAGE_OPTION].as<string>(); // global var
    assert(DD_PACKAGES.contains(ddPackage));

    threadCount = result[THREAD_COUNT_OPTION].as<Int>(); // global var
    if (threadCount <= 0) {
      threadCount = thread::hardware_concurrency();
    }
    assert(threadCount > 0);

    threadSliceCount = result[THREAD_SLICE_COUNT_OPTION].as<Int>(); // global var
    assert(threadSliceCount > 0);

    randomSeed = result[RANDOM_SEED_OPTION].as<Int>(); // global var

    ddVarOrderHeuristic = result[DD_VAR_OPTION].as<Int>();
    assert(CNF_VAR_ORDER_HEURISTICS.contains(abs(ddVarOrderHeuristic)));

    sliceVarOrderHeuristic = result[SLICE_VAR_OPTION].as<Int>();
    assert(util::getVarOrderHeuristics().contains(abs(sliceVarOrderHeuristic)));

    memSensitivity = result[MEM_SENSITIVITY_OPTION].as<Float>(); // global var
    maxMem = result[MAX_MEM_OPTION].as<Float>(); // global var

    tableRatio = result[TABLE_RATIO_OPTION].as<Int>();
    initRatio = result[INIT_RATIO_OPTION].as<Int>();

    multiplePrecision = result[MULTIPLE_PRECISION_OPTION].as<Int>(); // global var
    assert(!multiplePrecision || ddPackage == SYLVAN);

    logCounting = result[LOG_COUNTING_OPTION].as<Int>(); // global var
    assert(!logCounting || ddPackage == CUDD);

    joinPriority = result[JOIN_PRIORITY_OPTION].as<string>(); //global var
    assert(JOIN_PRIORITIES.contains(joinPriority));

    verboseCnf = result[VERBOSE_CNF_OPTION].as<Int>(); // global var
    verboseJoinTree = result[VERBOSE_JOIN_TREE_OPTION].as<Int>(); // global var

    verboseProfiling = result[VERBOSE_PROFILING_OPTION].as<Int>(); // global var
    assert(verboseProfiling <= 0 || threadCount == 1);

    verboseSolving = result[VERBOSE_SOLVING_OPTION].as<Int>(); // global var

    toolStartPoint = util::getTimePoint(); // global var
    runCommand();
    util::printRow("seconds", util::getDuration(toolStartPoint));
  }
  else {
    cout << options.help();
  }
}

/* global functions ========================================================= */

int main(int argc, char** argv) {
  cout << std::unitbuf; // enables automatic flushing
  OptionDict(argc, argv);
}
