import os


cmd_template = """./waf --run "nb-scenario4 --simDuration={sim_duration} \\
    --simName={sim_id} \\
    --num_ues={num_ues} \\
    --cell_size={cell_size} \\
    --ns3::LteUePhy::RsrpSinrSamplePeriod=1 \\
    --ns3::ConstantSpectrumPropagationLossModel::Loss={prop_loss} \\
    --ns3::LteUePhy::NoiseFigure={noise_figure} \\
    --ns3::LteUePhy::TxPower={tx_power_ue} \\
    --ns3::LteEnbPhy::NoiseFigure={noise_figure} \\
    --ns3::LteEnbPhy::TxPower={tx_power_enb}"
"""

if __name__ == "__main__":
    fname = "sh.run-sims"
    f = open(fname, "w")
    f.write("#!/bin/bash\n")


    prop_loss = 10
    sim_duration = 3600
    for cell_size in [2500]:
        for num_ues in [1, 2, 3, 4, 5, 10, 15, 20, 30, 40, 50, 60, 70, 80, 90, 100, 150, 200, 300, 400]:
            for noise_figure in [0, 10, 20]:
                for tx_power_ue in [10]:
                    for tx_power_enb in [30]:
                        sim_id = f"nb-scenario4-sim-{cell_size}m-{num_ues}ues-{noise_figure}nf-{tx_power_ue}ue-{tx_power_enb}enb"
                        cmd = cmd_template.format(
                            sim_duration=sim_duration,
                            sim_id=sim_id,
                            num_ues=num_ues,
                            cell_size=cell_size,
                            prop_loss=prop_loss,
                            noise_figure=noise_figure,
                            tx_power_ue=tx_power_ue,
                            tx_power_enb=tx_power_enb,
                        )
                        print(cmd)
                        f.write(cmd + "\n")

    f.close()
    os.chmod(fname, 0o755)
