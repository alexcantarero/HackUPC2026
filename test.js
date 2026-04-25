const stdout = "Best solution: score=81977  (training_score=81977)  time=0.505051s  bays=34  algorithm=sa\nOutput folder: ../data/output/1777149615_sa_81k\n";
const folderMatch = stdout.match(/Output folder:\s*(.+)/);
console.log(folderMatch);
